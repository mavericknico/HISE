/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licences for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licencing:
*
*   http://www.hartinstruments.net/hise/
*
*   HISE is based on the JUCE library,
*   which also must be licenced for commercial applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once

namespace snex {
namespace jit {
using namespace juce;
using namespace asmjit;

struct Operations::InlinedArgument : public Expression,
									 public SymbolStatement
{
	SET_EXPRESSION_ID(InlinedArgument);

	InlinedArgument(Location l, int argIndex_, const Symbol& s_, Ptr target):
		Expression(l),
		argIndex(argIndex_),
		s(s_)
	{
		addStatement(target);
	}

	Symbol getSymbol() const override
	{
		return s;
	}

	bool isConstExpr() const override
	{
		return getSubExpr(0)->isConstExpr();
	}

	VariableStorage getConstExprValue() const override
	{
		return getSubExpr(0)->getConstExprValue();
	}

	ValueTree toValueTree() const override
	{
		auto v = Expression::toValueTree();
		v.setProperty("Arg", argIndex, nullptr);
		v.setProperty("ParameterName", s.toString(), nullptr);
		//v.addChild(getSubExpr(0)->toValueTree(), -1, nullptr);

		return v;
	}

	Statement::Ptr clone(Location l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		auto n = new InlinedArgument(l, argIndex, s, c1);
		return n;
	}

	TypeInfo getTypeInfo() const override
	{
		return getSubExpr(0)->getTypeInfo();
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	int argIndex;
	Symbol s;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InlinedArgument);
};


struct Operations::StatementBlock : public Expression,
							        public ScopeStatementBase
{
	Identifier getStatementId() const override
	{
		if (isInlinedFunction)
		{
			RETURN_STATIC_IDENTIFIER("InlinedFunction");
		}
		else
		{
			RETURN_STATIC_IDENTIFIER("StatementBlock");
		}
	}

	StatementBlock(Location l, const NamespacedIdentifier& ns) :
		Expression(l),
		ScopeStatementBase(ns)
	{}

	TypeInfo getTypeInfo() const override { return returnType; };

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		Statement::Ptr c = new StatementBlock(l, getPath());

		auto b = dynamic_cast<StatementBlock*>(c.get());
		
		b->isInlinedFunction = isInlinedFunction;
		
		cloneScopeProperties(b);
		cloneChildren(c);

		return c;
	}

	ValueTree toValueTree() const override
	{
		auto v = Statement::toValueTree();
		v.setProperty("ScopeId", getPath().toString(), nullptr);
		return v;
	}

	static bool isRealStatement(Statement* s);

	bool isConstExpr() const override
	{
		int numStatements = 0;

		for (auto s : *this)
		{
			if (!s->isConstExpr())
				return false;
		}

		return true;
	}

	bool hasSideEffect() const override
	{
		return isInlinedFunction;
	}

	VariableStorage getConstExprValue() const override
	{
		int numStatements = getNumChildStatements();

		if (numStatements == 0)
			return VariableStorage(Types::ID::Void, 0);

		return getSubExpr(numStatements - 1)->getConstExprValue();
	}

	void addInlinedParameter(int index, const Symbol& s, Ptr e)
	{
		auto ia = new InlinedArgument(location, index, s, e);
		addStatement(ia, true);
	}

	static InlinedArgument* findInlinedParameterInParentBlocks(Statement* p, const Symbol& s)
	{
		if (p == nullptr)
			return nullptr;

		if (auto parentInlineArgument = findParentStatementOfType<InlinedArgument>(p))
		{
			auto parentBlock = findParentStatementOfType<StatementBlock>(parentInlineArgument);

			auto ipInParent = findInlinedParameterInParentBlocks(parentBlock->parent, s);

			if(ipInParent != nullptr)
				return ipInParent;

		}
			

		if (auto sb = dynamic_cast<StatementBlock*>(p))
		{
			if (sb->isInlinedFunction)
			{
				for (auto c : *sb)
				{
					if (auto ia = dynamic_cast<InlinedArgument*>(c))
					{
						if (ia->s == s)
							return ia;
					}
				}

				return nullptr;
			}
		}

		p = p->parent.get();

		if (p != nullptr)
			return findInlinedParameterInParentBlocks(p, s);

		return nullptr;
	}

	BaseScope* createOrGetBlockScope(BaseScope* parent)
	{
		if (parent->getScopeType() == BaseScope::Class)
			return parent;

		if (blockScope == nullptr)
			blockScope = new RegisterScope(parent, getPath());

		return blockScope;
	}

	void process(BaseCompiler* compiler, BaseScope* scope)
	{
		auto bs = createOrGetBlockScope(scope);

		processBaseWithChildren(compiler, bs);

		COMPILER_PASS(BaseCompiler::RegisterAllocation)
		{
			if (hasReturnType())
			{
				if (!isInlinedFunction)
				{
					allocateReturnRegister(compiler, bs);
				}
			}

			reg = returnRegister;
		}
	}

	ScopedPointer<RegisterScope> blockScope;
	bool isInlinedFunction = false;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatementBlock);
};

struct Operations::Noop : public Expression
{
	SET_EXPRESSION_ID(Noop);

	Noop(Location l) :
		Expression(l)
	{}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		return new Noop(l);
	}

	void process(BaseCompiler* compiler, BaseScope* scope)
	{
		processBaseWithoutChildren(compiler, scope);
	}

	TypeInfo getTypeInfo() const override { return {}; };

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Noop);
};


struct Operations::Immediate : public Expression
{
	SET_EXPRESSION_ID(Immediate);

	Immediate(Location loc, VariableStorage value) :
		Expression(loc),
		v(value)
	{};

	TypeInfo getTypeInfo() const override { return TypeInfo(v.getType(), true, false); }

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		return new Immediate(l, v);
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();
		t.setProperty("Value", var(Types::Helpers::getCppValueString(v)), nullptr);
		return t;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithoutChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			// We don't need to use the target register from the 
			// assignment for immediates
			reg = nullptr;

			reg = compiler->getRegFromPool(scope, getTypeInfo());
			reg->setDataPointer(v.getDataPointer(), true);

			reg->createMemoryLocation(getFunctionCompiler(compiler));
		}
	}

	VariableStorage v;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Immediate);
};



struct Operations::InlinedParameter : public Expression,
									  public SymbolStatement
{
	SET_EXPRESSION_ID(InlinedParameter);

	InlinedParameter(Location l, const Symbol& s_, Ptr source_):
		Expression(l),
		s(s_),
		source(source_)
	{}

	Statement::Ptr clone(Location l) const override;

	Symbol getSymbol() const override { return s; };

	ValueTree toValueTree() const override
	{
		auto v = Expression::toValueTree();
		v.setProperty("Symbol", s.toString(), nullptr);
		return v;
	}


	TypeInfo getTypeInfo() const override
	{
		return source->getTypeInfo();
	}

	bool isConstExpr() const override
	{
		return source->isConstExpr();
	}

	VariableStorage getConstExprValue() const override
	{
		return source->getConstExprValue();
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::RegisterAllocation)
		{
			reg = source->reg;
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			if (source->currentPass != BaseCompiler::CodeGeneration)
			{
				source->process(compiler, scope);
			}

			if (reg == nullptr)
				reg = source->reg;

			jassert(reg != nullptr);
		}
	}

	Symbol s;
	Ptr source;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InlinedParameter);
};

struct Operations::VariableReference : public Expression,
									   public SymbolStatement
{
	SET_EXPRESSION_ID(VariableReference);

	VariableReference(Location l, const Symbol& id_) :
		Expression(l),
		id(id_)
	{
		jassert(id);
	};


	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		return new VariableReference(l, id);
	}


	Symbol getSymbol() const override { return id; };

	ValueTree toValueTree() const override
	{
		auto t = Statement::toValueTree();
		t.setProperty("Symbol", id.toString(), nullptr);
		return t;
	}

	juce::String toString(Statement::TextFormat f) const override
	{
		return id.id.toString();
	}

	/** This scans the tree and checks whether it's the last reference.
	
		It ignores the control flow, so when the variable is part of a true
		branch, it might return true if the variable is used in the false
		branch.
	*/
	bool isLastVariableReference() const
	{
		SyntaxTreeWalker walker(this);

		auto lastOne = walker.getNextStatementOfType<VariableReference>();;

		bool isLast = lastOne == this;

		while (lastOne != nullptr)
		{
            auto isOtherVariable = lastOne->id != id;
            
            lastOne = walker.getNextStatementOfType<VariableReference>();
            
            if(isOtherVariable)
                continue;
            
            isLast = lastOne == this;
		}

		return isLast;
	}

	int getNumWriteAcesses()
	{
		int numWriteAccesses = 0;

		SyntaxTreeWalker walker(this);

		while (auto v = walker.getNextStatementOfType<VariableReference>())
		{
			if (v->id == id && v->isBeingWritten())
				numWriteAccesses++;
		}

		return numWriteAccesses;
	}

	/** This flags all variables that are not referenced later as ready for
	    reuse.
		
		The best place to call this is after a child statement was processed
		with the BaseCompiler::CodeGeneration pass.
		It makes sure that if the register is used by a parent expression that
		it will not be flagged for reuse (eg. when used as target register of
		a binary operation).
	*/
	static void reuseAllLastReferences(Statement::Ptr parentStatement)
	{
		ReferenceCountedArray<AssemblyRegister> parentRegisters;

		auto pExpr = dynamic_cast<Expression*>(parentStatement.get());

		while (pExpr != nullptr)
		{
			if (pExpr->reg != nullptr)
				parentRegisters.add(pExpr->reg);

			pExpr = dynamic_cast<Expression*>(pExpr->parent.get());
		}

		SyntaxTreeWalker w(parentStatement, false);

		while (auto v = w.getNextStatementOfType<VariableReference>())
		{
			if (parentRegisters.contains(v->reg))
				continue;

			if (v->isLastVariableReference())
			{
				if (v->parameterIndex != -1)
					continue;

				v->reg->flagForReuse();
			}
		}
	}

	bool tryToResolveType(BaseCompiler* c) override
	{
		if (id.resolved)
			return true;

		auto newType = c->namespaceHandler.getVariableType(id.id);

		if (!newType.isDynamic())
			id = { id.id, newType };

		return id.resolved;
	}

	bool isConstExpr() const override
	{
		return !id.constExprValue.isVoid();
	}

	TypeInfo getTypeInfo() const override
	{
		return id.typeInfo;
	}

	

	FunctionClass* getFunctionClassForParentSymbol(BaseScope* scope) const
	{
		if (id.id.getParent().isValid())
		{
			return scope->getRootData()->getSubFunctionClass(id.id.getParent());
		}

		return nullptr;
	}

	VariableStorage getConstExprValue() const override
	{
		return id.constExprValue;
	}

	bool isReferencedOnce() const
	{
		SyntaxTreeWalker w(this);

		int numReferences = 0;

		while(auto v = w.getNextStatementOfType<VariableReference>())
		{
			if (v->id == id)
				numReferences++;
		}

		return numReferences == 1;
	}

	bool isParameter(BaseScope* scope) const
	{
		if (auto fScope = dynamic_cast<FunctionScope*>(scope->getScopeForSymbol(id.id)))
		{
			return fScope->parameters.contains(id.getName());
		}

		return false;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	bool isBeingWritten()
	{
		return getWriteAccessType() != JitTokens::void_;
	}

	bool isInlinedParameter() const
	{
		return inlinedParameterExpression != nullptr;
	}

	TokenType getWriteAccessType();

	bool isClassVariable(BaseScope* scope) const
	{
		return scope->getRootClassScope()->rootData->contains(id.id);
	}

    bool isFirstReference()
	{
		SyntaxTreeWalker walker(this);

		while (auto v = walker.getNextStatementOfType<VariableReference>())
		{
			if (v->id == id && v->variableScope == variableScope)
				return v == this;
		}

		jassertfalse;
		return true;
	}

	bool validateLocalDefinition(BaseCompiler* compiler, BaseScope* scope)
	{
		jassert(isLocalDefinition);
		
		if (auto es = scope->getScopeForSymbol(id.id))
		{
			bool isAlreadyDefinedSubClassMember = false;

			if (auto cs = dynamic_cast<ClassScope*>(es))
			{
				isAlreadyDefinedSubClassMember = cs->typePtr != nullptr;
			}

			juce::String w;
			w << "declaration of " << id.toString() << " hides ";

			switch (es->getScopeType())
			{
			case BaseScope::Class:  w << "class member"; break;
			case BaseScope::Global: w << "global variable"; break;
			default:			    w << "previous declaration"; break;
			}

			if (!isAlreadyDefinedSubClassMember)
				logWarning(w);
		}

		// The type must have been set or it is a undefined variable
		if (getType() == Types::ID::Dynamic)
			location.throwError("Use of undefined variable " + id.toString());
        
        return true;
	}

	int parameterIndex = -1;

	Ptr inlinedParameterExpression;


	Symbol id;
	WeakReference<BaseScope> variableScope;
	bool isFirstOccurence = false;
	bool isLocalDefinition = false;

	// can be either the data pointer or the member offset
	VariableStorage objectAdress;
	ComplexType::WeakPtr objectPtr;

	// Contains the expression that leads to the pointer of the member object
	Ptr objectExpression;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VariableReference);


};


struct Operations::Cast : public Expression
{
	SET_EXPRESSION_ID(Cast);

	Cast(Location l, Expression::Ptr expression, Types::ID targetType_) :
		Expression(l)
	{
		addStatement(expression);
		targetType = TypeInfo(targetType_);
	};

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto cc = getSubExpr(0)->clone(l);
		return new Cast(l, cc, targetType.getType());
	}

	ValueTree toValueTree() const override
	{
		auto sourceType = getSubExpr(0)->getType();
		auto targetType = getType();

		auto t = Expression::toValueTree();
		t.setProperty("Source", Types::Helpers::getTypeName(sourceType), nullptr);
		t.setProperty("Target", Types::Helpers::getTypeName(targetType), nullptr);
		return t;
	}

	// Make sure the target type is used.
	TypeInfo getTypeInfo() const override { return targetType; }

	void process(BaseCompiler* compiler, BaseScope* scope);

	FunctionData complexCastFunction;
	TypeInfo targetType;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Cast);
};

struct Operations::DotOperator : public Expression
{
	DotOperator(Location l, Ptr parent, Ptr child):
		Expression(l)
	{
		addStatement(parent);
		addStatement(child);
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto cp = getSubExpr(0)->clone(l);
		auto cc = getSubExpr(1)->clone(l);
		return new DotOperator(l, cp, cc);
	}

	Identifier getStatementId() const override { RETURN_STATIC_IDENTIFIER("Dot"); }

	Ptr getDotParent() const
	{
		return getSubExpr(0);
	}

	Ptr getDotChild() const
	{
		return getSubExpr(1);
	}

	bool tryToResolveType(BaseCompiler* compiler) override
	{
		if (Statement::tryToResolveType(compiler))
			return true;

		if (getDotChild()->getTypeInfo().isInvalid())
		{
			if (auto st = getDotParent()->getTypeInfo().getTypedIfComplexType<StructType>())
			{
				if (auto ss = dynamic_cast<Operations::SymbolStatement*>(getDotChild().get()))
				{
					auto id = ss->getSymbol().getName();

					if (st->hasMember(id))
					{
						auto fullId = st->id.getChildId(id);
						
						location.test(compiler->namespaceHandler.checkVisiblity(fullId));

						resolvedType = st->getMemberTypeInfo(id);
						return true;
					}
				}
			}
		}

		return false;
	}

	TypeInfo getTypeInfo() const override
	{
		if (resolvedType.isValid())
			return resolvedType;

		return getDotChild()->getTypeInfo();
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	TypeInfo resolvedType;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DotOperator);
};
						

struct Operations::Assignment : public Expression,
								public TypeDefinitionBase
{
	enum class TargetType
	{
		Variable,
		Reference,
		Span,
		ClassMember,
		numTargetTypes
	};

	Assignment(Location l, Expression::Ptr target, TokenType assignmentType_, Expression::Ptr expr, bool firstAssignment_);

	Array<NamespacedIdentifier> getInstanceIds() const override { return { getTargetVariable()->id.id }; };

	TypeInfo getTypeInfo() const override { return getSubExpr(1)->getTypeInfo(); }

	Identifier getStatementId() const override { RETURN_STATIC_IDENTIFIER("Assignment"); }

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto ce = getSubExpr(0)->clone(l);
		auto ct = getSubExpr(1)->clone(l);
		return new Assignment(l, ct, assignmentType, ce, isFirstAssignment);
	}

	ValueTree toValueTree() const override
	{
		auto sourceType = getSubExpr(0)->getType();
		auto targetType = getType();

		auto t = Expression::toValueTree();
		t.setProperty("First", isFirstAssignment, nullptr);
		t.setProperty("AssignmentType", assignmentType, nullptr);
		
		return t;
	}

	bool hasSideEffect() const override
	{
		return true;
	}

	TargetType getTargetType() const;

	size_t getRequiredByteSize(BaseCompiler* compiler, BaseScope* scope) const override
	{
		
		if (scope->getScopeType() == BaseScope::Class && isFirstAssignment)
		{
			jassert(getSubExpr(0)->isConstExpr());
			return Types::Helpers::getSizeForType(getSubExpr(0)->getType());
		}
		
		return 0;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	VariableReference* getTargetVariable() const
	{
		auto targetType = getTargetType();
		jassert(targetType == TargetType::Variable || targetType == TargetType::Reference);
		auto v = getSubExpr(1).get();
		return dynamic_cast<VariableReference*>(v);
	}

    bool loadDataBeforeAssignment() const
    {
        if(assignmentType != JitTokens::assign_)
            return true;
        
        if(overloadedAssignOperator.isResolved())
            return true;
        
        return false;
    }
    
	DotOperator* getMemberTarget() const
	{
		jassert(getTargetType() == TargetType::ClassMember);
		return dynamic_cast<DotOperator*>(getSubExpr(1).get());
	}

	void initClassMembers(BaseCompiler* compiler, BaseScope* scope);

	TokenType assignmentType;
	bool isFirstAssignment = false;
	FunctionData overloadedAssignOperator;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Assignment);
};


struct Operations::Compare : public Expression
{
	Compare(Location location, Expression::Ptr l, Expression::Ptr r, TokenType op_) :
		Expression(location),
		op(op_)
	{
		addStatement(l);
		addStatement(r);
	};

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		auto c2 = getSubExpr(1)->clone(l);
		return new Compare(l, c1, c2, op);
	}

	Identifier getStatementId() const override { RETURN_STATIC_IDENTIFIER("Comparison"); }

	ValueTree toValueTree() const override
	{
		auto sourceType = getSubExpr(0)->getType();
		auto targetType = getType();

		auto t = Expression::toValueTree();
		t.setProperty("OpType", op, nullptr);

		return t;
	}

	TypeInfo getTypeInfo() const override { return TypeInfo(Types::Integer); }

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			auto l = getSubExpr(0);
			auto r = getSubExpr(1);

			if (l->getType() != r->getType())
			{
				Ptr implicitCast = new Operations::Cast(location, getSubExpr(1), l->getType());
				logWarning("Implicit cast to int for comparison");
				replaceChildStatement(1, implicitCast);
			}
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			{
				auto asg = CREATE_ASM_COMPILER(getType());

				auto l = getSubExpr(0);
				auto r = getSubExpr(1);

				reg = compiler->getRegFromPool(scope, getTypeInfo());

				auto tReg = getSubRegister(0);
				auto value = getSubRegister(1);

				asg.emitCompare(useAsmFlag, op, reg, l->reg, r->reg);

				VariableReference::reuseAllLastReferences(this);

				l->reg->flagForReuseIfAnonymous();
				r->reg->flagForReuseIfAnonymous();
			}
		}
	}

	TokenType op;
	bool useAsmFlag = false;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Compare);
};

struct Operations::LogicalNot : public Expression
{
	SET_EXPRESSION_ID(LogicalNot);

	LogicalNot(Location l, Ptr expr) :
		Expression(l)
	{
		addStatement(expr);
	};

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		return new LogicalNot(l, dynamic_cast<Expression*>(c1.get()));
	}

	TypeInfo getTypeInfo() const override { return TypeInfo(Types::ID::Integer); }

	void process(BaseCompiler* compiler, BaseScope* scope)
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			if (getSubExpr(0)->getType() != Types::ID::Integer)
				throwError("Wrong type for logic operation");
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto asg = CREATE_ASM_COMPILER(getType());

			reg = asg.emitLogicalNot(getSubRegister(0));
		}
	}

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogicalNot);
};

struct Operations::TernaryOp : public Expression,
							   public BranchingStatement
{
public:

	SET_EXPRESSION_ID(TernaryOp);

	TernaryOp(Location l, Ptr c, Ptr t, Ptr f) :
		Expression(l)
	{
		addStatement(c);
		addStatement(t);
		addStatement(f);
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		auto c2 = getSubExpr(1)->clone(l);
		auto c3 = getSubExpr(2)->clone(l);
		return new TernaryOp(l, c1, c2, c3);
	}

	TypeInfo getTypeInfo() const override
	{
		return type;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		// We need to have precise control over the code generation
		// for the subexpressions to avoid execution of both branches
		if (compiler->getCurrentPass() == BaseCompiler::CodeGeneration)
			processBaseWithoutChildren(compiler, scope);
		else
			processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			type = checkAndSetType(1, type);
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto asg = CREATE_ASM_COMPILER(getType());
			reg = asg.emitTernaryOp(this, compiler, scope);
			jassert(reg->isActive());
		}
	}

private:

	TypeInfo type;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TernaryOp);

};


struct Operations::FunctionCall : public Expression
{
	SET_EXPRESSION_ID(FunctionCall);

	enum CallType
	{
		Unresolved,
		InbuiltFunction,
		MemberFunction,
		StaticFunction,
		ExternalObjectFunction,
		RootFunction,
		GlobalFunction,
		ApiFunction,
		NativeTypeCall, // either block or event
		numCallTypes
	};



	FunctionCall(Location l, Ptr f, const Symbol& id, const Array<TemplateParameter>& tp) :
		Expression(l)
	{
		for (auto& p : tp)
		{
			jassert(!p.isTemplateArgument());
		}

		function.id = id.id;
		function.returnType = id.typeInfo;
		function.templateParameters = tp;

		if (auto dp = dynamic_cast<DotOperator*>(f.get()))
		{
			setObjectExpression(dp->getDotParent());
		}
	};

	void setObjectExpression(Ptr e)
	{
		if (hasObjectExpression)
		{
			getObjectExpression()->replaceInParent(e);
		}
		else
		{
			hasObjectExpression = true;
			addStatement(e.get(), true);
		}
	}

	Ptr getObjectExpression() const
	{
		if (hasObjectExpression)
			return getSubExpr(0);

		return nullptr;
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		

		auto newFC = new FunctionCall(l, nullptr, Symbol(function.id, function.returnType), function.templateParameters);

		if (getObjectExpression())
		{
			auto clonedObject = getObjectExpression()->clone(l);
			newFC->setObjectExpression(clonedObject);
		}

		for (int i = 0; i < getNumArguments(); i++)
		{
			newFC->addArgument(getArgument(i)->clone(l));
		}

		if (function.isResolved())
			newFC->function = function;

		return newFC;
	}

	bool tryToResolveType(BaseCompiler* compiler) override;

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();

		t.setProperty("Signature", function.getSignature(), nullptr);

		const StringArray resolveNames = { "Unresolved", "InbuiltFunction", "MemberFunction", "ExternalObjectFunction", "RootFunction", "GlobalFunction", "ApiFunction", "NativeTypeCall" };

		t.setProperty("CallType", resolveNames[(int)callType], nullptr);

		return t;
	}

	void addArgument(Ptr arg)
	{
		addStatement(arg);
	}

	Expression* getArgument(int index) const
	{
		return getSubExpr(!hasObjectExpression ? index : (index + 1));
	}

	int getNumArguments() const
	{
		if (!hasObjectExpression)
			return getNumChildStatements();
		else
			return getNumChildStatements() - 1;
	}

	bool hasSideEffect() const override
	{
		return true;
	}

	bool shouldInlineFunctionCall(BaseCompiler* compiler, BaseScope* scope) const;

	static bool canBeAliasParameter(Ptr e)
	{
		return dynamic_cast<VariableReference*>(e.get()) != nullptr;
	}

	void inlineFunctionCall(AsmCodeGenerator& acg);

	TypeInfo getTypeInfo() const override;

	void process(BaseCompiler* compiler, BaseScope* scope);

	CallType callType = Unresolved;
	Array<FunctionData> possibleMatches;
	mutable FunctionData function;
	WeakReference<FunctionClass> fc;
	FunctionClass::Ptr ownedFc;
	
	bool hasObjectExpression = false;

	ReferenceCountedArray<AssemblyRegister> parameterRegs;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunctionCall);

};

struct Operations::ThisPointer : public Statement
{
	SET_EXPRESSION_ID(ThisPointer);

	ThisPointer(Location l, TypeInfo t) :
		Statement(l),
		type(t.getComplexType().get())
	{

	}

	Statement::Ptr clone(Location l) const
	{
		return new ThisPointer(l, getTypeInfo());
	}

	TypeInfo getTypeInfo() const override
	{
		return TypeInfo(type.get());
	}

	ValueTree toValueTree() const override
	{
		auto v = Expression::toValueTree();
		v.setProperty("Type", getTypeInfo().toString(), nullptr);
		return v;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	ComplexType::WeakPtr type;
};

struct Operations::MemoryReference : public Expression
{
	SET_EXPRESSION_ID(MemoryReference);

	MemoryReference(Location l, Ptr base, const TypeInfo& type_, int offsetInBytes_):
		Expression(l),
		offsetInBytes(offsetInBytes_),
		type(type_)
	{
		addStatement(base);
	}

	Statement::Ptr clone(Location l) const
	{
		Statement::Ptr p = getSubExpr(0)->clone(l);
		return new MemoryReference(l, dynamic_cast<Expression*>(p.get()), type, offsetInBytes);
	}

	TypeInfo getTypeInfo() const override
	{
		return type;
	}

	ValueTree toValueTree() const override
	{
		auto v = Expression::toValueTree();
		v.setProperty("Offset", offsetInBytes, nullptr);
		return v;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);

		


		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto registerType = compiler->getRegisterType(type);


			auto baseReg = getSubRegister(0);

			reg = compiler->registerPool.getNextFreeRegister(scope, type);

			X86Mem ptr;

			if (baseReg->isMemoryLocation())
				ptr = baseReg->getAsMemoryLocation().cloneAdjustedAndResized(offsetInBytes, 8);
			else if (baseReg->isGlobalVariableRegister())
            {
                auto acg = CREATE_ASM_COMPILER(Types::ID::Pointer);
                auto b_ = acg.cc.newGpq();
                
                acg.cc.mov(b_, reinterpret_cast<int64_t>(baseReg->getGlobalDataPointer()) + (int64_t)offsetInBytes);
                
				ptr = x86::qword_ptr(b_);
            }
			else
				ptr = x86::ptr(PTR_REG_W(baseReg)).cloneAdjustedAndResized(offsetInBytes, 8);

            
            
			reg->setCustomMemoryLocation(ptr, true);

			reg = compiler->registerPool.getRegisterWithMemory(reg);
		}
	}

	int offsetInBytes = 0;
	TypeInfo type;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MemoryReference);
};

struct Operations::PointerAccess : public Expression
{
	SET_EXPRESSION_ID(PointerAccess);

	ValueTree toValueTree() const
	{
		return Statement::toValueTree();
	}

	Ptr clone(Location l) const override
	{
		return new PointerAccess(l, getSubExpr(0)->clone(l));
	}

	TypeInfo getTypeInfo() const
	{
		return getSubExpr(0)->getTypeInfo();
	}

	PointerAccess(Location l, Ptr target):
		Statement(l)
	{
		addStatement(target);
	}

	void process(BaseCompiler* compiler, BaseScope* s)
	{
		Statement::processBaseWithChildren(compiler, s);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			auto t = getTypeInfo();

			if (!t.isComplexType())
				throwError("Can't dereference non-complex type");
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			reg = compiler->registerPool.getNextFreeRegister(s, getTypeInfo());

			auto acg = CREATE_ASM_COMPILER(Types::ID::Pointer);

			auto obj = getSubRegister(0);

			auto mem = obj->getMemoryLocationForReference();

			jassert(!mem.isNone());

			auto ptrReg = acg.cc.newGpq();
			acg.cc.mov(ptrReg, mem);

			reg->setCustomMemoryLocation(x86::ptr(ptrReg), obj->isGlobalMemory());
		}

	}
};

struct Operations::ReturnStatement : public Expression
{
	ReturnStatement(Location l, Expression::Ptr expr) :
		Expression(l)
	{
		if (expr != nullptr)
			addStatement(expr);
	}

	Identifier getStatementId() const override
	{
		if (findInlinedRoot() != nullptr)
			return Identifier("InlinedReturnValue");
		else
			return Identifier("ReturnStatement");
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		Statement::Ptr p = !isVoid() ? getSubExpr(0)->clone(l) : nullptr;
		return new ReturnStatement(l, p);
	}

	bool isConstExpr() const override
	{
		return isVoid() || getSubExpr(0)->isConstExpr();
	}

	VariableStorage getConstExprValue() const override
	{
		return isVoid() ? VariableStorage(Types::ID::Void, 0) : getSubExpr(0)->getConstExprValue();
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();
		t.setProperty("Type", getTypeInfo().toString(), nullptr);
		return t;
	}

	TypeInfo getTypeInfo() const override
	{
		if (auto sl = ScopeStatementBase::getStatementListWithReturnType(const_cast<ReturnStatement*>(this)))
			return sl->getReturnType();

		jassertfalse;
		return {};
	}

	bool isVoid() const
	{
		return getTypeInfo() == Types::ID::Void;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			if (auto fScope = dynamic_cast<FunctionScope*>(findFunctionScope(scope)))
			{
				TypeInfo actualType(Types::ID::Void);

				if (auto first = getSubExpr(0))
					actualType = first->getTypeInfo();

				if (isVoid() && actualType != Types::ID::Void)
					throwError("Can't return a value from a void function.");
				if (!isVoid() && actualType == Types::ID::Void)
					throwError("function must return a value");
					
				checkAndSetType(0, getTypeInfo());
			}
			else
				throwError("Can't deduce return type.");
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto t = getTypeInfo().toPointerIfNativeRef();

			auto asg = CREATE_ASM_COMPILER(t.getType());

			bool isFunctionReturn = true;

			if (!isVoid())
			{
				if (auto sb = findInlinedRoot())
				{
					reg = getSubRegister(0);
					sb->reg = reg;

					if (reg != nullptr && reg->isActive())
						jassert(reg->isValid());
				}
				else if (auto sl = findRoot())
				{
					reg = sl->getReturnRegister();

					if (reg != nullptr && reg->isActive())
						jassert(reg->isValid());
				}

				if (reg == nullptr)
					throwError("Can't find return register");

				if (reg->isActive())
					jassert(reg->isValid());
			}

			if (findInlinedRoot() == nullptr)
			{
				auto sourceReg = isVoid() ? nullptr : getSubRegister(0);

				asg.emitReturn(compiler, reg, sourceReg);
			}
			else
			{
				asg.writeDirtyGlobals(compiler);
			}
		}
	}

	ScopeStatementBase* findRoot() const
	{
		return ScopeStatementBase::getStatementListWithReturnType(const_cast<ReturnStatement*>(this));
	}

	StatementBlock* findInlinedRoot() const
	{
		if (auto sl = findRoot())
		{
			if (auto sb = dynamic_cast<StatementBlock*>(sl))
			{
				if (sb->isInlinedFunction)
				{
					return sb;
				}
			}
		}

		return nullptr;
	};

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReturnStatement);
};

struct Operations::ClassStatement : public Statement,
									public Operations::ClassDefinitionBase
{
	SET_EXPRESSION_ID(ClassStatement)

	ClassStatement(Location l, ComplexType::Ptr classType_, Statement::Ptr classBlock) :
		Statement(l),
		classType(classType_)
	{
		addStatement(classBlock);

	}

	~ClassStatement()
	{
		classType = nullptr;
		int x = 5;
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		jassertfalse;
		return nullptr;
	}

	ValueTree toValueTree() const override
	{
		auto t = Statement::toValueTree();

		t.setProperty("Type", classType->toString(), nullptr);

		
		return t;
	}

	bool isTemplate() const override { return false; }

	TypeInfo getTypeInfo() const override { return {}; }

	size_t getRequiredByteSize(BaseCompiler* compiler, BaseScope* scope) const override
	{
		jassert(compiler->getCurrentPass() > BaseCompiler::ComplexTypeParsing);
		return classType->getRequiredByteSize();
	}

	void process(BaseCompiler* compiler, BaseScope* scope);

	StructType* getStructType()
	{
		return dynamic_cast<StructType*>(classType.get());
	}

	ComplexType::Ptr classType;
	ScopedPointer<ClassScope> subClass;
};


struct Operations::TemplateDefinition : public Statement,
										public ClassDefinitionBase
{
	SET_EXPRESSION_ID(TemplateDefinition);

	TemplateDefinition(Location l, const NamespacedIdentifier& classId, NamespaceHandler& handler_, Statement::Ptr statements_) :
		Statement(l),
		templateClassId(classId),
		statements(statements_),
		handler(handler_)
	{
		for (const auto& p : getTemplateArguments())
		{
			jassert(p.isTemplateArgument());
		}
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		auto cp = compiler->getCurrentPass();

		Statement::processBaseWithoutChildren(compiler, scope);

		if (cp == BaseCompiler::ComplexTypeParsing || cp == BaseCompiler::FunctionParsing)
		{
			for (auto c : *this)
			{
				auto tip = collectParametersFromParentClass(c, {});
				TemplateParameterResolver resolver(tip);
				auto r = resolver.process(c);

				if (!r.wasOk())
					throwError(r.getErrorMessage());
			}
		}

		Statement::processAllChildren(compiler, scope);
	}

	ValueTree toValueTree() const override
	{
		auto t = Statement::toValueTree();

		juce::String s;
		s << templateClassId.toString();
		s << TemplateParameter::ListOps::toString(getTemplateArguments());

		t.setProperty("Type", s, nullptr);

		return t;
	}

	bool isTemplate() const override
	{
		return true;
	}

	TemplateParameter::List getTemplateArguments() const
	{
		
		return handler.getTemplateObject(templateClassId).argList;
	}

	Statement::Ptr clone(Location l) const override
	{
		auto cs = statements->clone(l);

		auto s = new TemplateDefinition(l, templateClassId, handler, cs);

		clones.add(s);

		return s;
	}

	TypeInfo getTypeInfo() const override
	{
		return {};
	}

	ComplexType::Ptr createTemplate(const TemplateObject::ConstructData& d)
	{
		auto instanceParameters = TemplateParameter::ListOps::merge(getTemplateArguments(), d.tp, *d.r);

		for (auto es : *this)
		{
			if (auto ecs = as<ClassStatement>(es))
			{
				auto tp = ecs->getStructType()->getTemplateInstanceParameters();

				if (TemplateParameter::ListOps::match(instanceParameters, tp))
				{
					return ecs->getStructType();
				}
			}
		}

		for (auto c : clones)
		{
			auto p = as<TemplateDefinition>(c)->createTemplate(d);
		}

		if (d.r->failed())
			throwError(d.r->getErrorMessage());

		ComplexType::Ptr p = new StructType(templateClassId, instanceParameters);

		p = handler.registerComplexTypeOrReturnExisting(p);

		Ptr cb = new SyntaxTree(location, as<ScopeStatementBase>(statements)->getPath());

		statements->cloneChildren(cb);

		auto c = new ClassStatement(location, p, cb);

		addStatement(c);

		if (currentCompiler != nullptr)
		{
			TemplateParameterResolver resolver(instanceParameters);
			resolver.process(c);

			c->currentCompiler = currentCompiler;
			c->processAllPassesUpTo(currentPass, currentScope);
		}

		return d.handler->registerComplexTypeOrReturnExisting(p);
	}

	NamespacedIdentifier templateClassId;
	NamespaceHandler& handler;

	Statement::Ptr statements;

	mutable List clones;
};



struct Operations::Function : public Statement,
	public asmjit::ErrorHandler,
	public Operations::FunctionDefinitionBase
{
	SET_EXPRESSION_ID(Function);

	Function(Location l, const Symbol& id_) :
		Statement(l),
		FunctionDefinitionBase(id_)
	{
		
	};

	~Function()
	{
		data = {};
		functionScope = nullptr;
		statements = nullptr;
		parameters.clear();
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		jassert(functionScope == nullptr);
		jassert(functionClass == nullptr);
		jassert(statements == nullptr);
		jassert(objectPtr == nullptr);
		
		auto c = new Function(l, { data.id, data.returnType });

		c->data = data;

		c->code = code;
		c->codeLength = codeLength;
		c->parameters = parameters;

		return c;
	}

	ValueTree toValueTree() const override
	{
		auto t = Statement::toValueTree();
		t.setProperty("Signature", data.getSignature(parameters), nullptr);

		if (classData != nullptr && classData->function != nullptr)
		{
			t.setProperty("FuncPointer", reinterpret_cast<int64>(classData->function), nullptr);
		}
			

		if(statements != nullptr)
			t.addChild(statements->toValueTree(), -1, nullptr);

		return t;
	}

	void handleError(asmjit::Error, const char* message, asmjit::BaseEmitter* emitter) override
	{
		throwError(juce::String(message));
	}

	TypeInfo getTypeInfo() const override { return TypeInfo(data.returnType); }

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	

	FunctionClass::Ptr functionClass;

	ScopedPointer<FunctionScope> functionScope;
	
	//Symbol id;
	RegPtr objectPtr;
	bool hasObjectPtr;
	FunctionData* classData = nullptr;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Function);
};

struct Operations::TemplatedFunction : public Statement,
								       public FunctionDefinitionBase
{
	SET_EXPRESSION_ID(TemplatedFunction);

	TemplatedFunction(Location l, const Symbol& s, const TemplateParameter::List& tp):
		Statement(l),
		FunctionDefinitionBase(s),
		templateParameters(tp)
	{
		for (auto& l : templateParameters)
		{
			jassert(l.isTemplateArgument());

			if (!s.id.isParentOf(l.argumentId))
			{
				jassert(l.argumentId.isExplicit());
				l.argumentId = s.id.getChildId(l.argumentId.getIdentifier());
			}
		}
	}

	void createFunction(const TemplateObject::ConstructData& d)
	{
		Result r = Result::ok();
		auto instanceParameters = TemplateParameter::ListOps::merge(templateParameters, d.tp, r);
		location.test(r);

		//DBG("Creating template function " + d.id.toString() + TemplateParameter::ListOps::toString(templateParameters));
		
		if (currentCompiler != nullptr)
		{
			auto currentParameters = currentCompiler->namespaceHandler.getCurrentTemplateParameters();
			location.test(TemplateParameter::ListOps::expandIfVariadicParameters(instanceParameters, currentParameters));
			instanceParameters = TemplateParameter::ListOps::merge(templateParameters, instanceParameters, *d.r);
			//DBG("Resolved template parameters: " + TemplateParameter::ListOps::toString(instanceParameters));

			if (instanceParameters.size() < templateParameters.size())
			{
				// Shouldn't happen, the parseCall() method should have resolved the template parameters 
				// to another function already...
				jassertfalse;
			}
		}

		TemplateParameterResolver resolve(collectParametersFromParentClass(this, instanceParameters));

		for (auto e : *this)
		{
			if (auto ef = as<Function>(e))
			{
				auto fParameters = ef->data.templateParameters;

				if (TemplateParameter::ListOps::match(fParameters, instanceParameters))
					return;// ef->data;
			}
		}

		for (auto c : clones)
			as<TemplatedFunction>(c)->createFunction(d);
		
		FunctionData fData = data;

		resolve.resolveIds(fData);

		fData.templateParameters = instanceParameters;

		auto newF = new Function(location, {});
		newF->code = code;
		newF->codeLength = codeLength;
		newF->data = fData;
		newF->parameters = parameters;
		
		addStatement(newF);

		auto isInClass = findParentStatementOfType<ClassStatement>(this) != nullptr;
		
		auto ok = resolve.process(newF);

		if (isInClass)
		{
			
			location.test(ok);
		}
		

		if (currentCompiler != nullptr)
		{
			NamespaceHandler::ScopedTemplateParameterSetter stps(currentCompiler->namespaceHandler, instanceParameters);
			newF->currentCompiler = currentCompiler;
			newF->processAllPassesUpTo(currentPass, currentScope);
		}
		
		return;// fData;
	}

	Ptr clone(Location l) const override
	{
		Ptr f = new TemplatedFunction(l, { data.id, data.returnType }, templateParameters);
		as<TemplatedFunction>(f)->parameters = parameters;
		as<TemplatedFunction>(f)->code = code;
		as<TemplatedFunction>(f)->codeLength = codeLength;
		
		cloneChildren(f);

		clones.add(f);

		return f;
	}

	ValueTree toValueTree() const override
	{
		auto v = Statement::toValueTree();

		return v;
	}

	TypeInfo getTypeInfo() const override { return {}; }

	Function* getFunctionWithTemplateAmount(const NamespacedIdentifier& id, int numTemplateParameters)
	{
		for (auto f_ : *this)
		{
			auto f = as<Function>(f_);

			if (id == f->data.id && f->data.templateParameters.size() == numTemplateParameters)
				return f;
		}

		// Now we'll have to look at the parent syntax tree
		SyntaxTreeWalker w(this);

		while (auto tf = w.getNextStatementOfType<TemplatedFunction>())
		{
			if (tf == this)
				continue;

			if (tf->data.id == id)
			{
				auto list = tf->collectFunctionInstances();

				for (auto f_ : list)
				{
					auto f = as<Function>(f_);

					if (id == f->data.id && f->data.templateParameters.size() == numTemplateParameters)
						return f;
				}
			}
		}

		return nullptr;
	}

	Statement::List collectFunctionInstances()
	{
		Statement::List orderedFunctions;

		auto id = data.id;

		for (auto f_ : *this)
		{
			auto f = as<Function>(f_);

			int numProvided = f->data.templateParameters.size();
			DBG("Scanning " + f->data.getSignature({}) + "for recursive function calls");

			f->statements->forEachRecursive([id, numProvided, this, &orderedFunctions](Ptr p)
			{
				if (auto fc = as<FunctionCall>(p))
				{
					if (fc->function.id == id)
					{
						DBG("Found recursive function call " + fc->function.getSignature({}));
						auto numThisF = fc->function.templateParameters.size();

						if (auto rf = getFunctionWithTemplateAmount(id, numThisF))
							orderedFunctions.addIfNotAlreadyThere(rf);
					}
				}

				return false;
			});

			orderedFunctions.addIfNotAlreadyThere(f);
		}

		return orderedFunctions;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		Statement::processBaseWithoutChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::FunctionCompilation)
		{
			if (TemplateParameter::ListOps::isVariadicList(templateParameters))
			{
				auto list = collectFunctionInstances();

				struct Sorter
				{
					static int compareElements(Ptr first, Ptr second)
					{
						auto f1 = as<Function>(first);
						auto f2 = as<Function>(second);
						auto s1 = f1->data.templateParameters.size();
						auto s2 = f2->data.templateParameters.size();

						if (s1 > s2)      return 1;
						else if (s1 < s2) return -1;
						else              return 0;
					}
				};

				Sorter s;
				list.sort(s);

				for (auto l : list)
					l->process(compiler, scope);

				return;
			}

		}

		Statement::processAllChildren(compiler, scope);
		
	}

	TemplateParameter::List templateParameters;

	mutable Statement::List clones;
};

struct Operations::BinaryOp : public Expression
{
	SET_EXPRESSION_ID(BinaryOp);
	
	BinaryOp(Location l, Expression::Ptr left, Expression::Ptr right, TokenType opType) :
		Expression(l),
		op(opType)
	{
		addStatement(left);
		addStatement(right);
	};

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		auto c2 = getSubExpr(1)->clone(l);

		return new BinaryOp(l, c1, c2, op);
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();
		
		t.setProperty("OpType", op, nullptr);
		t.setProperty("UseTempRegister", usesTempRegister, nullptr);
		return t;
	}

	bool isLogicOp() const { return op == JitTokens::logicalOr || op == JitTokens::logicalAnd; }

	TypeInfo getTypeInfo() const override
	{
		return getSubExpr(0)->getTypeInfo();
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		// Defer evaluation of the children for operators with short circuiting...
		bool processChildren = !(isLogicOp() && (compiler->getCurrentPass() == BaseCompiler::CodeGeneration));

		if (processChildren)
			processBaseWithChildren(compiler, scope);
		else
			processBaseWithoutChildren(compiler, scope);

		if (isLogicOp() && getSubExpr(0)->isConstExpr())
		{
			bool isOr1 = op == JitTokens::logicalOr && getSubExpr(0)->getConstExprValue().toInt() == 1;
			bool isAnd0 = op == JitTokens::logicalAnd && getSubExpr(0)->getConstExprValue().toInt() == 0;

			if (isOr1 || isAnd0)
			{
				replaceInParent(getSubExpr(0));
				return;
			}
		}

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			if (op == JitTokens::logicalAnd ||
				op == JitTokens::logicalOr)
			{
				checkAndSetType(0, TypeInfo(Types::ID::Integer));
			}
			else
				checkAndSetType(0, {});
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto asg = CREATE_ASM_COMPILER(getType());

			if (isLogicOp())
			{
				asg.emitLogicOp(this);
			}
			else
			{
				auto l = getSubRegister(0);

				if (auto childOp = dynamic_cast<BinaryOp*>(getSubExpr(0).get()))
				{
					if (childOp->usesTempRegister)
						l->flagForReuse();
				}

				usesTempRegister = false;

				if (l->canBeReused())
				{
					reg = l;
					reg->removeReuseFlag();
					jassert(!reg->isMemoryLocation());
				}
				else
				{
					if (reg == nullptr)
					{
						asg.emitComment("temp register for binary op");
						reg = compiler->getRegFromPool(scope, getTypeInfo());
						usesTempRegister = true;
					}

					asg.emitStore(reg, getSubRegister(0));
				}

				auto le = getSubExpr(0);
				auto re = getSubExpr(1);

				asg.emitBinaryOp(op, reg, getSubRegister(1));

				VariableReference::reuseAllLastReferences(getChildStatement(0));
				VariableReference::reuseAllLastReferences(getChildStatement(1));
			}
		}
	}

	bool usesTempRegister = false;
	TokenType op;
};

struct Operations::UnaryOp : public Expression
{
	UnaryOp(Location l, Ptr expr) :
		Expression(l)
	{
		addStatement(expr);
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);
	}
};



struct Operations::Increment : public UnaryOp
{
	SET_EXPRESSION_ID(Increment);

	Increment(Location l, Ptr expr, bool isPre_, bool isDecrement_) :
		UnaryOp(l, expr),
		isPreInc(isPre_),
		isDecrement(isDecrement_)
	{
	};

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);

		return new Increment(l, c1, isPreInc, isDecrement);
	}

	TypeInfo getTypeInfo() const override
	{
		return getSubExpr(0)->getTypeInfo();
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();

		t.setProperty("IsPre", isPreInc, nullptr);
		t.setProperty("IsDec", isDecrement, nullptr);

		return t;
	}

	bool hasSideEffect() const override
	{
		return true;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::SyntaxSugarReplacements)
		{
			if (removed)
				return;
		}

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			if (dynamic_cast<Increment*>(getSubExpr(0).get()) != nullptr)
				throwError("Can't combine incrementors");

			if(compiler->getRegisterType(getTypeInfo()) != Types::ID::Integer)
				throwError("Can't increment non integer variables.");
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto asg = CREATE_ASM_COMPILER(getType());

			RegPtr valueReg;
			RegPtr dataReg = getSubRegister(0);

			if (!isPreInc)
				valueReg = compiler->getRegFromPool(scope, TypeInfo(Types::ID::Integer));

			bool done = false;

			if (getTypeInfo().isComplexType())
			{
				FunctionClass::Ptr fc = getTypeInfo().getComplexType()->getFunctionClass();
				auto f = fc->getSpecialFunction(FunctionClass::IncOverload, getTypeInfo(), {TypeInfo(Types::ID::Integer, false, true)});

				if (f.canBeInlined(false))
				{
					getOrSetIncProperties(f.templateParameters, isPreInc, isDecrement);
					AssemblyRegister::List l;

					l.add(valueReg);
					asg.emitFunctionCall(dataReg, f, nullptr, l);
					done = true;
				}
			}
			
			if(!done)
				asg.emitIncrement(valueReg, dataReg, isPreInc, isDecrement);

			if (isPreInc)
				reg = dataReg;
			else
				reg = valueReg;

			jassert(reg != nullptr);
		}
	}

	static void getOrSetIncProperties(Array<TemplateParameter>& tp, bool& isPre, bool& isDec)
	{
		if (tp.isEmpty())
		{
			TemplateParameter d;
			d.constant = (int)isDec;
			TemplateParameter p;
			p.constant = (int)isPre;

			tp.add(d);
			tp.add(p);
		}
		else
		{
			isDec = tp[0].constant;
			isPre = tp[1].constant;
		}
	}

	bool isDecrement;
	bool isPreInc;
	bool removed = false;
};

struct Operations::WhileLoop : public Statement,
	public Operations::ConditionalBranch
{
	SET_EXPRESSION_ID(WhileLoop);

	WhileLoop(Location l, Ptr condition, Ptr body):
		Statement(l)
	{
		addStatement(condition);
		addStatement(body);
	}

	ValueTree toValueTree() const override
	{
		auto v = Statement::toValueTree();

		return v;
	}

	Statement::Ptr clone(Location l) const override
	{
		auto c = getSubExpr(0)->clone(l);
		auto b = getSubExpr(1)->clone(l);

		auto w = new WhileLoop(l, c, b);

		return w;
	}

	TypeInfo getTypeInfo() const override
	{
		jassertfalse;
		return {};
	}

	Compare* getCompareCondition()
	{
		if (auto cp = as<Compare>(getSubExpr(0)))
			return cp;

		if (auto sb = as<StatementBlock>(getSubExpr(0)))
		{
			for (auto s : *sb)
			{
				if (auto cb = as<ConditionalBranch>(s))
					return nullptr;

				if (auto rt = as<ReturnStatement>(s))
				{
					return as<Compare>(rt->getSubExpr(0));
				}
			}
		}

		return nullptr;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		

		if(compiler->getCurrentPass() == BaseCompiler::CodeGeneration)
			Statement::processBaseWithoutChildren(compiler, scope);
		else
			Statement::processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			if (getSubExpr(0)->isConstExpr())
			{
				auto v = getSubExpr(0)->getConstExprValue();

				if (v.toInt() != 0)
				{
					throwError("Endless loop detected");
				}
			}
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			

			auto acg = CREATE_ASM_COMPILER(Types::ID::Integer);

			auto safeCheck = scope->getGlobalScope()->isRuntimeErrorCheckEnabled();

			auto cond = acg.cc.newLabel();
			auto exit = acg.cc.newLabel();

			auto why = acg.cc.newGpd();

			if (safeCheck)
				acg.cc.xor_(why, why);

			acg.cc.nop();

			acg.cc.bind(cond);
			
			auto cp = getCompareCondition();

			if (cp != nullptr)
				cp->useAsmFlag = true;

			getSubExpr(0)->process(compiler, scope);
			auto cReg = getSubRegister(0);

			dumpSyntaxTree(this);

			if (cp != nullptr)
			{
#define INT_COMPARE(token, command) if (cp->op == token) command(exit);

				INT_COMPARE(JitTokens::greaterThan, acg.cc.jle);
				INT_COMPARE(JitTokens::lessThan, acg.cc.jge);
				INT_COMPARE(JitTokens::lessThanOrEqual, acg.cc.jg);
				INT_COMPARE(JitTokens::greaterThanOrEqual, acg.cc.jl);
				INT_COMPARE(JitTokens::equals, acg.cc.jne);
				INT_COMPARE(JitTokens::notEquals, acg.cc.je);

#undef INT_COMPARE

				if (safeCheck)
				{
					acg.cc.inc(why);
					acg.cc.cmp(why, 10000000);
					auto okBranch = acg.cc.newLabel();
					acg.cc.jb(okBranch);

					

					auto errorFlag = x86::ptr(scope->getGlobalScope()->getRuntimeErrorFlag()).cloneResized(4);
					acg.cc.mov(why, (int)RuntimeError::ErrorType::WhileLoop);
					acg.cc.mov(errorFlag, why);
					acg.cc.mov(why, (int)location.getLine());
					acg.cc.mov(errorFlag.cloneAdjustedAndResized(4, 4), why);
					acg.cc.mov(why, (int)location.getColNumber(location.program, location.location));
					acg.cc.mov(errorFlag.cloneAdjustedAndResized(8, 4), why);
					acg.cc.jmp(exit);
					acg.cc.bind(okBranch);
				}
			}
			else
			{
				acg.cc.setInlineComment("check condition");
				acg.cc.cmp(INT_REG_R(cReg), 0);
				acg.cc.je(exit);

				if (safeCheck)
				{
					acg.cc.inc(why);
					acg.cc.cmp(why, 10000000);
					auto okBranch = acg.cc.newLabel();
					acg.cc.jb(okBranch);

					auto errorFlag = x86::ptr(scope->getGlobalScope()->getRuntimeErrorFlag()).cloneResized(4);
					acg.cc.mov(why, (int)RuntimeError::ErrorType::WhileLoop);
					acg.cc.mov(errorFlag, why);
					acg.cc.mov(why, (int)location.getLine());
					acg.cc.mov(errorFlag.cloneAdjustedAndResized(4, 4), why);
					acg.cc.mov(why, (int)location.getColNumber(location.program, location.location));
					acg.cc.mov(errorFlag.cloneAdjustedAndResized(8, 4), why);
					acg.cc.jmp(exit);
					acg.cc.bind(okBranch);
				}
			}

			getSubExpr(1)->process(compiler, scope);

			acg.cc.jmp(cond);
			acg.cc.bind(exit);
		}
	}
};

struct Operations::Loop : public Expression,
					      public Operations::ConditionalBranch,
						  public Operations::ArrayStatementBase
{
	SET_EXPRESSION_ID(Loop);

	
	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		auto c2 = getSubExpr(1)->clone(l);

		auto newLoop = new Loop(l, iterator, c1, c2);
		return newLoop;
	}

	ArrayType getArrayType() const override
	{
		return loopTargetType;
	}

	Loop(Location l, const Symbol& it_, Expression::Ptr t, Statement::Ptr body) :
		Expression(l),
		iterator(it_)
	{
		addStatement(t);
		addStatement(body);

		jassert(dynamic_cast<StatementBlock*>(body.get()) != nullptr);
	};

	~Loop()
	{
		int x = 5;
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();

		static const StringArray loopTypes = { "Undefined", "Span", "Block", "CustomObject" };
		t.setProperty("LoopType", loopTypes[loopTargetType], nullptr);
		t.setProperty("LoadIterator", loadIterator, nullptr);
		t.setProperty("Iterator", iterator.toString(), nullptr);
		
		return t;
	}

	TypeInfo getTypeInfo() const override { return {}; }

	Expression::Ptr getTarget()
	{
		return getSubExpr(0);
	}

	StatementBlock* getLoopBlock()
	{
		auto b = getChildStatement(1);

		return dynamic_cast<StatementBlock*>(b.get());
	}

	bool tryToResolveType(BaseCompiler* compiler) override
	{
		getTarget()->tryToResolveType(compiler);

		auto tt = getTarget()->getTypeInfo();

		if (auto targetType = tt.getTypedIfComplexType<ArrayTypeBase>())
		{
			auto r = compiler->namespaceHandler.setTypeInfo(iterator.id, NamespaceHandler::Variable, targetType->getElementType());

			auto iteratorType = targetType->getElementType().withModifiers(iterator.isConst(), iterator.isReference());
			
			iterator = { iterator.id, iteratorType };

			if (r.failed())
				throwError(r.getErrorMessage());
		}

		Statement::tryToResolveType(compiler);

		return true;
	}

	bool evaluateIteratorStore()
	{
		if (storeIterator)
			return true;

		SyntaxTreeWalker w(getLoopBlock(), false);

		while (auto v = w.getNextStatementOfType<VariableReference>())
		{
			if (v->id == iterator)
			{
				if (v->parent->hasSideEffect())
				{
					if (auto a = as<Assignment>(v->parent.get()))
					{
						if (a->getSubExpr(0).get() == v)
							continue;
					}

					storeIterator = true;
					break;
				}
			}
		}

		return storeIterator;
	}

	bool evaluateIteratorLoad()
	{
		if (!loadIterator)
			return false;

		SyntaxTreeWalker w(getLoopBlock(), false);

		while (auto v = w.getNextStatementOfType<VariableReference>())
		{
			if (v->id == iterator)
			{
				if (auto a = findParentStatementOfType<Assignment>(v))
				{
					if (a->getSubExpr(1).get() == v && a->assignmentType == JitTokens::assign_)
					{
						auto sId = v->id;

						bool isSelfAssign = a->getSubExpr(0)->forEachRecursive([sId](Operations::Statement::Ptr p)
						{
							if (auto v = dynamic_cast<VariableReference*>(p.get()))
							{
								if (v->id == sId)
									return true;
							}

							return false;
						});

						loadIterator = isSelfAssign;
					}

					if (a->assignmentType != JitTokens::assign_)
						loadIterator = true;

					if (a->getSubExpr(1).get() != v)
						loadIterator = true;
				}

				break;
			}
		}

		return loadIterator;
	}

	void process(BaseCompiler* compiler, BaseScope* scope)
	{
		processBaseWithoutChildren(compiler, scope);

		if (compiler->getCurrentPass() != BaseCompiler::DataAllocation &&
			compiler->getCurrentPass() != BaseCompiler::CodeGeneration)
		{

			getTarget()->process(compiler, scope);
			getLoopBlock()->process(compiler, scope);
		}

		COMPILER_PASS(BaseCompiler::DataAllocation)
		{
			tryToResolveType(compiler);

			getTarget()->process(compiler, scope);

			auto targetType = getTarget()->getTypeInfo();

			if (auto sp = targetType.getTypedIfComplexType<SpanType>())
			{
				loopTargetType = Span;

				if (iterator.typeInfo.isDynamic())
					iterator.typeInfo = sp->getElementType();
				else if (iterator.typeInfo != sp->getElementType())
					location.throwError("iterator type mismatch: " + iterator.typeInfo.toString() + " expected: " + sp->getElementType().toString());
			}
			else if (auto dt = targetType.getTypedIfComplexType<DynType>())
			{
				loopTargetType = Dyn;

				if (iterator.typeInfo.isDynamic())
					iterator.typeInfo = dt->elementType;
				else if (iterator.typeInfo != dt->elementType)
					location.throwError("iterator type mismatch: " + iterator.typeInfo.toString() + " expected: " + sp->getElementType().toString());
			}
			else if (targetType.getType() == Types::ID::Block)
			{
				loopTargetType = Dyn;

				if (iterator.typeInfo.isDynamic())
					iterator.typeInfo = TypeInfo(Types::ID::Float, iterator.isConst(), iterator.isReference());
				else if (iterator.typeInfo.getType() != Types::ID::Float)
					location.throwError("Illegal iterator type");
			}
			else
			{
				if (auto st = targetType.getTypedIfComplexType<StructType>())
				{
					FunctionClass::Ptr fc = st->getFunctionClass();

					customBegin = fc->getSpecialFunction(FunctionClass::BeginIterator);
					customSizeFunction = fc->getSpecialFunction(FunctionClass::SizeFunction);

					if (!customBegin.isResolved() || !customSizeFunction.isResolved())
						throwError(st->toString() + " does not have iterator methods");

					

					loopTargetType = CustomObject;

					if (iterator.typeInfo.isDynamic())
						iterator.typeInfo = customBegin.returnType;
					else if (iterator.typeInfo != customBegin.returnType)
						location.throwError("iterator type mismatch: " + iterator.typeInfo.toString() + " expected: " + customBegin.returnType.toString());

				}
				else
				{
					throwError("Can't deduce loop target type");
				}

				
			}
				
			
			compiler->namespaceHandler.setTypeInfo(iterator.id, NamespaceHandler::Variable, iterator.typeInfo);
			
			getLoopBlock()->process(compiler, scope);

			evaluateIteratorLoad();
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto acg = CREATE_ASM_COMPILER(compiler->getRegisterType(iterator.typeInfo));

			getTarget()->process(compiler, scope);

			auto t = getTarget();

			auto r = getTarget()->reg;

			jassert(r != nullptr && r->getScope() != nullptr);

			allocateDirtyGlobalVariables(getLoopBlock(), compiler, scope);

			if (loopTargetType == Span)
			{
				auto le = new SpanLoopEmitter(compiler, iterator, getTarget()->reg, getLoopBlock(), loadIterator);
				le->typePtr = getTarget()->getTypeInfo().getTypedComplexType<SpanType>();
				loopEmitter = le;
			}
			else if (loopTargetType == Dyn)
			{
				auto le = new DynLoopEmitter(compiler, iterator, getTarget()->reg, getLoopBlock(), loadIterator);
				le->typePtr = getTarget()->getTypeInfo().getTypedComplexType<DynType>();
				loopEmitter = le;
			}
			else if (loopTargetType == CustomObject)
			{
				auto le = new CustomLoopEmitter(compiler, iterator, getTarget()->reg, getLoopBlock(), loadIterator);
				le->beginFunction = customBegin;
				le->sizeFunction = customSizeFunction;
				loopEmitter = le;
			}
			
			if(loopEmitter != nullptr)
				loopEmitter->emitLoop(acg, compiler, scope);
		}
	}

	RegPtr iteratorRegister;
	Symbol iterator;
	
	bool loadIterator = true;
	bool storeIterator = false;

	ArrayType loopTargetType;

	asmjit::Label loopStart;
	asmjit::Label loopEnd;

	FunctionData customBegin;
	FunctionData customSizeFunction;

	ScopedPointer<AsmCodeGenerator::LoopEmitterBase> loopEmitter;

	JUCE_DECLARE_WEAK_REFERENCEABLE(Loop);
};

struct Operations::ControlFlowStatement : public Expression
{
	

	ControlFlowStatement(Location l, bool isBreak_):
		Expression(l),
		isBreak(isBreak_)
	{

	}

	Identifier getStatementId() const override
	{
		if (isBreak)
		{
			RETURN_STATIC_IDENTIFIER("break");
		}
		else
		{
			RETURN_STATIC_IDENTIFIER("continue");
		}
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		return new ControlFlowStatement(l, isBreak);
	}

	TypeInfo getTypeInfo() const override
	{
		return {};
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			parentLoop = findParentStatementOfType<Loop>(this);

			if (parentLoop == nullptr)
			{
				juce::String s;
				s << "a " << getStatementId().toString() << " may only be used within a loop or switch";
				throwError(s);
			}
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto acg = CREATE_ASM_COMPILER(Types::ID::Integer);
			acg.emitLoopControlFlow(parentLoop, isBreak);
		}
	};

	WeakReference<Loop> parentLoop;
	bool isBreak;
};


struct Operations::Negation : public Expression
{
	SET_EXPRESSION_ID(Negation);

	Negation(Location l, Expression::Ptr e) :
		Expression(l)
	{
		addStatement(e);
	};

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);

		return new Negation(l, dynamic_cast<Expression*>(c1.get()));
	}

	TypeInfo getTypeInfo() const override
	{
		return getSubExpr(0)->getTypeInfo();
	}

	void process(BaseCompiler* compiler, BaseScope* scope)
	{
		processBaseWithChildren(compiler, scope);

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			if (!isConstExpr())
			{
				auto asg = CREATE_ASM_COMPILER(getType());

				reg = compiler->getRegFromPool(scope, getTypeInfo());

				asg.emitNegation(reg, getSubRegister(0));

				getSubRegister(0)->flagForReuseIfAnonymous();
			}
			else
			{
				// supposed to be optimized away by now...
				jassertfalse;
			}
		}
	}
};

struct Operations::IfStatement : public Statement,
								 public Operations::ConditionalBranch,
								 public Operations::BranchingStatement
{
	SET_EXPRESSION_ID(IfStatement);

	IfStatement(Location loc, Expression::Ptr cond, Ptr trueBranch, Ptr falseBranch):
		Statement(loc)
	{
		addStatement(cond.get());
		addStatement(trueBranch);

		if (falseBranch != nullptr)
			addStatement(falseBranch);
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getChildStatement(0)->clone(l);
		auto c2 = getChildStatement(1)->clone(l);
		Statement::Ptr c3 = hasFalseBranch() ? getChildStatement(2)->clone(l) : nullptr;

		return new IfStatement(l, dynamic_cast<Expression*>(c1.get()), c2, c3);
	}

	TypeInfo getTypeInfo() const override { return {}; }

	bool hasFalseBranch() const { return getNumChildStatements() > 2; }
	
	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		processBaseWithoutChildren(compiler, scope);

		if (compiler->getCurrentPass() != BaseCompiler::CodeGeneration)
			processAllChildren(compiler, scope);
		
		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			processAllChildren(compiler, scope);

			if (getCondition()->getTypeInfo() != Types::ID::Integer)
				throwError("Condition must be boolean expression");
		}

		COMPILER_PASS(BaseCompiler::CodeGeneration)
		{
			auto acg = CREATE_ASM_COMPILER(Types::ID::Integer);

			allocateDirtyGlobalVariables(getTrueBranch(), compiler, scope);

			if(hasFalseBranch())
				allocateDirtyGlobalVariables(getFalseBranch(), compiler, scope);
			
			auto cond = dynamic_cast<Expression*>(getCondition().get());
			auto trueBranch = getTrueBranch();
			auto falseBranch = getFalseBranch();

			acg.emitBranch(TypeInfo(Types::ID::Void), cond, trueBranch, falseBranch, compiler, scope);
		}
	}
};





struct Operations::Subscript : public Expression,
							   public ArrayStatementBase
{
	SET_EXPRESSION_ID(Subscript);

	Subscript(Location l, Ptr expr, Ptr index):
		Expression(l)
	{
		addStatement(expr);
		addStatement(index);
	}

	Statement::Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		auto c1 = getSubExpr(0)->clone(l);
		auto c2 = getSubExpr(1)->clone(l);

		auto newSubscript = new Subscript(l, dynamic_cast<Expression*>(c1.get()), dynamic_cast<Expression*>(c2.get()));
		newSubscript->elementType = elementType;
		newSubscript->isWriteAccess = isWriteAccess;
		
		return newSubscript;
	}

	TypeInfo getTypeInfo() const override
	{
		return elementType;
	}

	ArrayType getArrayType() const override
	{
		return subscriptType;
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();

		t.setProperty("Write", isWriteAccess, nullptr);
		t.setProperty("ElementType", elementType.toString(), nullptr);
		t.setProperty("ParentType", getSubExpr(0)->getTypeInfo().toString(), nullptr);

		return t;
	}

	bool tryToResolveType(BaseCompiler* compiler) override
	{
		Statement::tryToResolveType(compiler);

		auto parentType = getSubExpr(0)->getTypeInfo();

		if (spanType = parentType.getTypedIfComplexType<SpanType>())
		{
			subscriptType = Span;
			elementType = spanType->getElementType();
			return true;
		}
		else if (dynType = parentType.getTypedIfComplexType<DynType>())
		{
			subscriptType = Dyn;
			elementType = dynType->elementType;
			return true;
		}
		else if (getSubExpr(0)->getType() == Types::ID::Block)
		{
			subscriptType = Dyn;
			elementType = TypeInfo(Types::ID::Float, false, true);
			return true;
		}
		else if (auto st = parentType.getTypedIfComplexType<StructType>())
		{
			FunctionClass::Ptr fc = st->getFunctionClass();

			if (fc->hasSpecialFunction(FunctionClass::Subscript))
			{
				subscriptOperator = fc->getSpecialFunction(FunctionClass::Subscript);
				subscriptType = CustomObject;
				elementType = subscriptOperator.returnType;
				return true;
			}
		}

		return false;
	}

	void process(BaseCompiler* compiler, BaseScope* scope)
	{
		processChildrenIfNotCodeGen(compiler, scope);

		COMPILER_PASS(BaseCompiler::DataAllocation)
		{
			tryToResolveType(compiler);
		}

		COMPILER_PASS(BaseCompiler::TypeCheck)
		{
			getSubExpr(1)->tryToResolveType(compiler);
			auto indexType = getSubExpr(1)->getTypeInfo();

			if (indexType.getType() != Types::ID::Integer)
			{
				if (auto it = indexType.getTypedIfComplexType<IndexBase>())
				{
					if (subscriptType == CustomObject)
					{
						auto wId = NamespacedIdentifier("IndexType").getChildId("wrapped");

						auto fData = compiler->getInbuiltFunctionClass()->getNonOverloadedFunctionRaw(wId);



					}
					else
					{
						auto parentType = getSubExpr(0)->getTypeInfo();

						if (TypeInfo(it->parentType.get()) != parentType)
						{
							juce::String s;

							s << "index type mismatch: ";
							s << indexType.toString();
							s << " for target ";
							s << parentType.toString();

							getSubExpr(1)->throwError(s);
						}
					}
				}
				else
					getSubExpr(1)->throwError("illegal index type");
			}
			else if (dynType == nullptr && !getSubExpr(1)->isConstExpr())
			{
				getSubExpr(1)->throwError("Can't use non-constant or non-wrapped index");
			}
			
			if (spanType != nullptr)
			{
				auto size = spanType->getNumElements();

				if (getSubExpr(1)->isConstExpr())
				{
					int index = getSubExpr(1)->getConstExprValue().toInt();

					if (!isPositiveAndBelow(index, size))
						getSubExpr(1)->throwError("constant index out of bounds");
				}
			}
			else if (dynType != nullptr)
			{
				// nothing to do here...
				return;
			}
			else if (subscriptType == CustomObject)
			{
				// nothing to do here, the type check will be done in the function itself...
				return;
			}
			else
			{
				if (getSubExpr(0)->getType() == Types::ID::Block)
					elementType = TypeInfo(Types::ID::Float, false, true);
				else
					getSubExpr(1)->throwError("Can't use []-operator");
			}
		}

		
		if(isCodeGenPass(compiler))
		{
			auto abortFunction = [this]()
			{
				return false;

				if (auto fc = dynamic_cast<FunctionCall*>(parent.get()))
				{
					if (fc->getObjectExpression().get() == this)
						return false;
				}


				if(dynamic_cast<SymbolStatement*>(getSubExpr(0).get()) == nullptr)
					return false;

				if (!getSubExpr(1)->isConstExpr())
					return false;

				if (getSubExpr(1)->getTypeInfo().isComplexType())
					return false;

				if (subscriptType == Dyn || subscriptType == ArrayStatementBase::CustomObject)
					return false;

				if (SpanType::isSimdType(getSubExpr(0)->getTypeInfo()))
					return false;

				return true;
			};

			if (!preprocessCodeGenForChildStatements(compiler, scope, abortFunction))
				return;

			if (subscriptType == Span && compiler->fitsIntoNativeRegister(getSubExpr(0)->getTypeInfo().getComplexType()))
			{
				reg = getSubRegister(0);
				return;
			}

			reg = compiler->registerPool.getNextFreeRegister(scope, getTypeInfo());

			auto tReg = getSubRegister(0);

			auto acg = CREATE_ASM_COMPILER(compiler->getRegisterType(getTypeInfo()));

			if (!subscriptOperator.isResolved())
			{
				auto cType = getSubRegister(0)->getTypeInfo().getTypedIfComplexType<ComplexType>();

				if (cType != nullptr)
				{
					if (FunctionClass::Ptr fc = cType->getFunctionClass())
					{
						subscriptOperator = fc->getSpecialFunction(FunctionClass::Subscript, elementType, { getSubRegister(0)->getTypeInfo(), getSubRegister(1)->getTypeInfo() });
					}
				}
			}

			if (subscriptOperator.isResolved())
			{
				AssemblyRegister::List l;
				l.add(getSubRegister(0));
				l.add(getSubRegister(1));

				acg.location = getSubExpr(1)->location;

				auto r = acg.emitFunctionCall(reg, subscriptOperator, nullptr, l);

				if (!r.wasOk())
					location.throwError(r.getErrorMessage());

				return;
			}

			RegPtr indexReg;

			indexReg = getSubRegister(1);
			jassert(indexReg->getType() == Types::ID::Integer);

			acg.emitSpanReference(reg, getSubRegister(0), indexReg, elementType.getRequiredByteSize());

			replaceMemoryWithExistingReference(compiler);
		}
	}

	ArrayType subscriptType = Undefined;
	bool isWriteAccess = false;
	SpanType* spanType = nullptr;
	DynType* dynType = nullptr;
	TypeInfo elementType;
	FunctionData subscriptOperator;
};





struct Operations::ComplexTypeDefinition : public Expression,
											public TypeDefinitionBase
{
	SET_EXPRESSION_ID(ComplexTypeDefinition);

	ComplexTypeDefinition(Location l, const Array<NamespacedIdentifier>& ids_, TypeInfo type_) :
		Expression(l),
		ids(ids_),
		type(type_)
	{}

	void addInitValues(InitialiserList::Ptr l)
	{
		initValues = l;

		initValues->forEach([this](InitialiserList::ChildBase* b)
		{
			if (auto ec = dynamic_cast<InitialiserList::ExpressionChild*>(b))
			{
				this->addStatement(ec->expression);
			}

			return false;
		});
	}

	Array<NamespacedIdentifier> getInstanceIds() const override { return ids; }

	Ptr clone(ParserHelpers::CodeLocation l) const override
	{
		Ptr n = new ComplexTypeDefinition(l, ids, type);

		cloneChildren(n);

		if (initValues != nullptr)
			as<ComplexTypeDefinition>(n)->initValues = initValues;

		return n;
	}

	TypeInfo getTypeInfo() const override
	{
		return type;
	}

	ValueTree toValueTree() const override
	{
		auto t = Expression::toValueTree();

		juce::String names;

		for (auto id : ids)
			names << id.toString() << ",";

		t.setProperty("Type", type.toString(), nullptr);

		t.setProperty("Ids", names, nullptr);
		
		if (initValues != nullptr)
			t.setProperty("InitValues", initValues->toString(), nullptr);

		return t;
	}

	Array<Symbol> getSymbols() const
	{
		Array<Symbol> symbols;

		for (auto id : ids)
		{
			symbols.add({ id, getTypeInfo() });
		}

		return symbols;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override;

	bool isStackDefinition(BaseScope* scope) const
	{
		return dynamic_cast<RegisterScope*>(scope) != nullptr;
	}



	Array<NamespacedIdentifier> ids;
	TypeInfo type;



	InitialiserList::Ptr initValues;

	ReferenceCountedArray<AssemblyRegister> stackLocations;
};


}
}
