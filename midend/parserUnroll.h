/*
Copyright 2016 VMware, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _MIDEND_PARSERUNROLL_H_
#define _MIDEND_PARSERUNROLL_H_

#include "ir/ir.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/callGraph.h"
#include "interpreter.h"

namespace P4 {

class ParserStructure {
    struct Work {
        ValueMap* valueMap;
        const IR::ParserState* state;
        const Work* derivedFrom;

        Work(ValueMap* valueMap, const IR::ParserState* state, const Work* derivedFrom) :
                valueMap(valueMap), state(state), derivedFrom(derivedFrom)
        { CHECK_NULL(valueMap); CHECK_NULL(state); }

        ValueMap* evaluate(ReferenceMap* refMap, TypeMap* typeMap) const;
        void evaluate(const IR::StatOrDecl* stat, ReferenceMap* refMap, TypeMap* typeMap) const;
    };

    std::vector<Work*> toRun;
    void run(ReferenceMap* refMap, TypeMap* typeMap);
    ValueMap* initializeVariables(TypeMap* typeMap);

 public:
    ExpressionEvaluator*              evaluator;
    const IR::P4Parser*               parser;
    CallGraph<const IR::ParserState*> callGraph;
    const IR::ParserState*            start;
    explicit ParserStructure(const IR::P4Parser* parser) :
            parser(parser), callGraph("parsers"), start(nullptr)
    { CHECK_NULL(parser); }

    void evaluate(ReferenceMap* refMap, TypeMap* typeMap);
};

class Parsers {
 public:
    std::map<const IR::P4Parser*, ParserStructure*> parsers;
};

class ParserAnalyzer : public Inspector {
    const ReferenceMap* refMap;
    ParserStructure*    current;
    Parsers*            parsers;
 public:
    ParserAnalyzer(const ReferenceMap* refMap, Parsers* parsers) :
            refMap(refMap), current(nullptr), parsers(parsers)
    { CHECK_NULL(refMap); CHECK_NULL(parsers); }
    void postorder(const IR::PathExpression* expression) override;
    bool preorder(const IR::Type*) override { return false; }  // prune
    bool preorder(const IR::P4Parser* parser) override
    { current = new ParserStructure(parser); return true; }  // do not prune
    void postorder(const IR::P4Parser* parser) override
    { parsers->parsers.emplace(parser, current); }
    bool preorder(const IR::ParserState* state) override {
        if (state->name.name == IR::ParserState::start)
            current->start = state;
        return true;
    }
};

// Process a parser and optionally perform unrolling of loops.
// Should be performed after local variable declarations have been removed.
class ParserEvaluator : public Transform {
    ReferenceMap*  refMap;
    TypeMap*       typeMap;
    const Parsers* parsers;
    bool           unroll;  // if true perform loop unrolling
 public:
    ParserEvaluator(ReferenceMap* refMap, TypeMap* typeMap, const Parsers* parsers) :
            refMap(refMap), typeMap(typeMap), parsers(parsers), unroll(false)
    { CHECK_NULL(refMap); CHECK_NULL(typeMap); CHECK_NULL(parsers); setName("ParserEvaluator"); }
    const IR::Node* postorder(IR::P4Parser* parser) override;
    const IR::Node* preorder(IR::P4Parser* parser) override { return parser; }
    const IR::Node* preorder(IR::Type* type) override { prune(); return type; }
};

class ParserUnroll : public PassManager {
    Parsers parsers;
 public:
    ParserUnroll(ReferenceMap* refMap, TypeMap* typeMap, bool isv1) {
        passes.push_back(new TypeChecking(refMap, typeMap, false, isv1));
        passes.push_back(new ParserAnalyzer(refMap, &parsers));
        passes.push_back(new ParserEvaluator(refMap, typeMap, &parsers));
        setName("ParserUnroll");
    }
};

}  // namespace P4

#endif /* _MIDEND_PARSERUNROLL_H_ */