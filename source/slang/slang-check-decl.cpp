// slang-check-decl.cpp
#include "slang-check-impl.h"

// This file constaints the semantic checking logic and
// related queries for declarations.
//
// Because declarations are the top-level construct
// of the AST (in turn containing all the statements,
// types, and expressions), the declaration-checking
// logic also orchestrates the overall flow and how
// and when things get checked.

#include "slang-lookup.h"

namespace Slang
{
        /// Visitor to transition declarations to `DeclCheckState::CheckedModifiers`
    struct SemanticsDeclModifiersVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclModifiersVisitor>
    {
        SemanticsDeclModifiersVisitor(SharedSemanticsContext* shared)
            : SemanticsDeclVisitorBase(shared)
        {}

        void visitDeclGroup(DeclGroup*) {}

        void visitDecl(Decl* decl)
        {
            checkModifiers(decl);
        }
    };

    struct SemanticsDeclHeaderVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclHeaderVisitor>
    {
        SemanticsDeclHeaderVisitor(SharedSemanticsContext* shared)
            : SemanticsDeclVisitorBase(shared)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        void checkVarDeclCommon(VarDeclBase* varDecl);

        void visitVarDecl(VarDecl* varDecl)
        {
            checkVarDeclCommon(varDecl);
        }

        void visitGlobalGenericValueParamDecl(GlobalGenericValueParamDecl* decl)
        {
            checkVarDeclCommon(decl);
        }

        void visitImportDecl(ImportDecl* decl);

        void visitGenericTypeParamDecl(GenericTypeParamDecl* decl);

        void visitGenericValueParamDecl(GenericValueParamDecl* decl);

        void visitGenericTypeConstraintDecl(GenericTypeConstraintDecl* decl);

        void visitGenericDecl(GenericDecl* genericDecl);

        void visitTypeDefDecl(TypeDefDecl* decl);

        void visitGlobalGenericParamDecl(GlobalGenericParamDecl* decl);

        void visitAssocTypeDecl(AssocTypeDecl* decl);

        void checkCallableDeclCommon(CallableDecl* decl);

        void visitFuncDecl(FuncDecl* funcDecl);

        void visitParamDecl(ParamDecl* paramDecl);

        void visitConstructorDecl(ConstructorDecl* decl);

        void visitSubscriptDecl(SubscriptDecl* decl);

        void visitAccessorDecl(AccessorDecl* decl);
    };

    struct SemanticsDeclRedeclarationVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclRedeclarationVisitor>
    {
        SemanticsDeclRedeclarationVisitor(SharedSemanticsContext* shared)
            : SemanticsDeclVisitorBase(shared)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

#define CASE(TYPE) void visit##TYPE(TYPE* decl) { checkForRedeclaration(decl); }

        CASE(FuncDecl)
        CASE(VarDeclBase)
        CASE(SimpleTypeDecl)
        CASE(AggTypeDecl)

#undef CASE
    };

    struct SemanticsDeclBasesVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclBasesVisitor>
    {
        SemanticsDeclBasesVisitor(SharedSemanticsContext* shared)
            : SemanticsDeclVisitorBase(shared)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        void visitInheritanceDecl(InheritanceDecl* inheritanceDecl);

        void visitAggTypeDecl(AggTypeDecl* decl);

        void visitEnumDecl(EnumDecl* decl);

        void visitExtensionDecl(ExtensionDecl* decl);
    };

    struct SemanticsDeclBodyVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclBodyVisitor>
    {
        SemanticsDeclBodyVisitor(SharedSemanticsContext* shared)
            : SemanticsDeclVisitorBase(shared)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        void checkVarDeclCommon(VarDeclBase* varDecl);

        void visitVarDecl(VarDecl* varDecl)
        {
            checkVarDeclCommon(varDecl);
        }

        void visitGlobalGenericValueParamDecl(GlobalGenericValueParamDecl* decl)
        {
            checkVarDeclCommon(decl);
        }

        void visitEnumCaseDecl(EnumCaseDecl* decl);

        void visitEnumDecl(EnumDecl* decl);

        void visitFunctionDeclBase(FunctionDeclBase* funcDecl);

        void visitParamDecl(ParamDecl* paramDecl);
    };

        /// Should the given `decl` nested in `parentDecl` be treated as a static rather than instance declaration?
    bool isEffectivelyStatic(
        Decl*           decl,
        ContainerDecl*  parentDecl)
    {
        // Things at the global scope are always "members" of their module.
        //
        if(as<ModuleDecl>(parentDecl))
            return false;

        // Anything explicitly marked `static` and not at module scope
        // counts as a static rather than instance declaration.
        //
        if(decl->hasModifier<HLSLStaticModifier>())
            return true;

        // Next we need to deal with cases where a declaration is
        // effectively `static` even if the language doesn't make
        // the user say so. Most languages make the default assumption
        // that nested types are `static` even if they don't say
        // so (Java is an exception here, perhaps due to some
        // influence from the Scandanavian OOP tradition of Beta/gbeta).
        //
        if(as<AggTypeDecl>(decl))
            return true;
        if(as<SimpleTypeDecl>(decl))
            return true;

        // Initializer/constructor declarations are effectively `static`
        // in Slang. They behave like functions that return an instance
        // of the enclosing type, rather than as functions that are
        // called on a pre-existing value.
        //
        if(as<ConstructorDecl>(decl))
            return true;

        // Things nested inside functions may have dependencies
        // on values from the enclosing scope, but this needs to
        // be dealt with via "capture" so they are also effectively
        // `static`
        //
        if(as<FunctionDeclBase>(parentDecl))
            return true;

        // Type constraint declarations are used in member-reference
        // context as a form of casting operation, so we treat them
        // as if they are instance members. This is a bit of a hack,
        // but it achieves the result we want until we have an
        // explicit representation of up-cast operations in the
        // AST.
        //
        if(as<TypeConstraintDecl>(decl))
            return false;

        return false;
    }

    bool isEffectivelyStatic(
        Decl*           decl)
    {
        // For the purposes of an ordinary declaration, when determining if
        // it is static or per-instance, the "parent" declaration we really
        // care about is the next outer non-generic declaration.
        //
        // TODO: This idiom of getting the "next outer non-generic declaration"
        // comes up just enough that we should probably have a convenience
        // function for it.

        auto parentDecl = decl->parentDecl;
        if(auto genericDecl = as<GenericDecl>(parentDecl))
            parentDecl = genericDecl->parentDecl;

        return isEffectivelyStatic(decl, parentDecl);
    }

        /// Is `decl` a global shader parameter declaration?
    bool isGlobalShaderParameter(VarDeclBase* decl)
    {
        // A global shader parameter must be declared at global or namespace
        // scope, so that it has a single definition across the module.
        //
        if(!as<NamespaceDeclBase>(decl->parentDecl)) return false;

        // A global variable marked `static` indicates a traditional
        // global variable (albeit one that is implicitly local to
        // the translation unit)
        //
        if(decl->hasModifier<HLSLStaticModifier>()) return false;

        // The `groupshared` modifier indicates that a variable cannot
        // be a shader parameters, but is instead transient storage
        // allocated for the duration of a thread-group's execution.
        //
        if(decl->hasModifier<HLSLGroupSharedModifier>()) return false;

        return true;
    }

    static bool _isLocalVar(VarDeclBase* varDecl)
    {
        auto pp = varDecl->parentDecl;

        if(as<ScopeDecl>(pp))
            return true;

        if(auto genericDecl = as<GenericDecl>(pp))
            pp = genericDecl;

        if(as<FuncDecl>(pp))
            return true;

        return false;
    }

    // Get the type to use when referencing a declaration
    QualType getTypeForDeclRef(
        ASTBuilder*             astBuilder,
        SemanticsVisitor*       sema,
        DiagnosticSink*         sink,
        DeclRef<Decl>           declRef,
        Type**           outTypeResult,
        SourceLoc               loc)
    {
        if( sema )
        {
            // Hack: if we are somehow referencing a local variable declaration
            // before the line of code that defines it, then we need to diagnose
            // an error.
            //
            // TODO: The right answer is that lookup should have been performed in
            // the scope that was in place *before* the variable was declared, but
            // this is a quick fix that at least alerts the user to how we are
            // interpreting their code.
            //
            // We detect the problematic case by looking for an attempt to reference
            // a local variable declaration when it is unchecked, or in the process
            // of being checked (the latter case catches a local variable that refers
            // to itself in its initial-value expression).
            //
            auto checkStateExt = declRef.getDecl()->checkState;
            if( checkStateExt.getState() == DeclCheckState::Unchecked
                || checkStateExt.isBeingChecked() )
            {
                if(auto varDecl = as<VarDecl>(declRef.getDecl()))
                {
                    if(_isLocalVar(varDecl))
                    {
                        sema->getSink()->diagnose(varDecl, Diagnostics::localVariableUsedBeforeDeclared, varDecl);
                        return QualType(astBuilder->getErrorType());
                    }
                }
            }

            // Once we've rules out the case of referencing a local declaration
            // before it has been checked, we will go ahead and ensure that
            // semantic checking has been performed on the chosen declaration,
            // at least up to the point where we can query its type.
            //
            sema->ensureDecl(declRef, DeclCheckState::CanUseTypeOfValueDecl);
        }

        // We need to insert an appropriate type for the expression, based on
        // what we found.
        if (auto varDeclRef = declRef.as<VarDeclBase>())
        {
            QualType qualType;
            qualType.type = getType(astBuilder, varDeclRef);

            bool isLValue = true;
            if(varDeclRef.getDecl()->findModifier<ConstModifier>())
                isLValue = false;

            // Global-scope shader parameters should not be writable,
            // since they are effectively program inputs.
            //
            // TODO: We could eventually treat a mutable global shader
            // parameter as a shorthand for an immutable parameter and
            // a global variable that gets initialized from that parameter,
            // but in order to do so we'd need to support global variables
            // with resource types better in the back-end.
            //
            if(isGlobalShaderParameter(varDeclRef.getDecl()))
                isLValue = false;

            // Variables declared with `let` are always immutable.
            if(varDeclRef.is<LetDecl>())
                isLValue = false;

            // Generic value parameters are always immutable
            if(varDeclRef.is<GenericValueParamDecl>())
                isLValue = false;

            // Function parameters declared in the "modern" style
            // are immutable unless they have an `out` or `inout` modifier.
            if(varDeclRef.is<ModernParamDecl>())
            {
                // Note: the `inout` modifier AST class inherits from
                // the class for the `out` modifier so that we can
                // make simple checks like this.
                //
                if( !varDeclRef.getDecl()->hasModifier<OutModifier>() )
                {
                    isLValue = false;
                }
            }

            qualType.isLeftValue = isLValue;
            return qualType;
        }
        else if( auto enumCaseDeclRef = declRef.as<EnumCaseDecl>() )
        {
            QualType qualType;
            qualType.type = getType(astBuilder, enumCaseDeclRef);
            qualType.isLeftValue = false;
            return qualType;
        }
        else if (auto typeAliasDeclRef = declRef.as<TypeDefDecl>())
        {
            auto type = getNamedType(astBuilder, typeAliasDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto aggTypeDeclRef = declRef.as<AggTypeDecl>())
        {
            auto type = DeclRefType::create(astBuilder, aggTypeDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto simpleTypeDeclRef = declRef.as<SimpleTypeDecl>())
        {
            auto type = DeclRefType::create(astBuilder, simpleTypeDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto genericDeclRef = declRef.as<GenericDecl>())
        {
            auto type = getGenericDeclRefType(astBuilder, genericDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto funcDeclRef = declRef.as<CallableDecl>())
        {
            auto type = getFuncType(astBuilder, funcDeclRef);
            return QualType(type);
        }
        else if (auto constraintDeclRef = declRef.as<TypeConstraintDecl>())
        {
            // When we access a constraint or an inheritance decl (as a member),
            // we are conceptually performing a "cast" to the given super-type,
            // with the declaration showing that such a cast is legal.
            auto type = getSup(astBuilder, constraintDeclRef);
            return QualType(type);
        }
        else if( auto namespaceDeclRef = declRef.as<NamespaceDeclBase>())
        {
            auto type = getNamespaceType(astBuilder, namespaceDeclRef);
            return QualType(type);
        }
        if( sink )
        {
            // The compiler is trying to form a reference to a declaration
            // that doesn't appear to be usable as an expression or type.
            //
            // In practice, this arises when user code has an undefined-identifier
            // error, but the name that was undefined in context also matches
            // a contextual keyword. Rather than confuse the user with the
            // details of contextual keywords in the compiler, we will diagnose
            // this as an undefined identifier.
            //
            // TODO: This code could break if we ever go down this path with
            // an identifier that doesn't have a name.
            //
            sink->diagnose(loc, Diagnostics::undefinedIdentifier2, declRef.getName());
        }
        return QualType(astBuilder->getErrorType());
    }

    QualType getTypeForDeclRef(
        ASTBuilder*     astBuilder, 
        DeclRef<Decl>   declRef,
        SourceLoc       loc)
    {
        Type* typeResult = nullptr;
        return getTypeForDeclRef(astBuilder, nullptr, nullptr, declRef, &typeResult, loc);
    }

    DeclRef<ExtensionDecl> ApplyExtensionToType(
        SemanticsVisitor*       semantics,
        ExtensionDecl*          extDecl,
        Type*  type)
    {
        if(!semantics)
            return DeclRef<ExtensionDecl>();

        return semantics->ApplyExtensionToType(extDecl, type);
    }

    GenericSubstitution* createDefaultSubstitutionsForGeneric(
        ASTBuilder*             astBuilder, 
        GenericDecl*            genericDecl,
        Substitutions*   outerSubst)
    {
        GenericSubstitution* genericSubst = astBuilder->create<GenericSubstitution>();
        genericSubst->genericDecl = genericDecl;
        genericSubst->outer = outerSubst;

        for( auto mm : genericDecl->members )
        {
            if( auto genericTypeParamDecl = as<GenericTypeParamDecl>(mm) )
            {
                genericSubst->args.add(DeclRefType::create(astBuilder, DeclRef<Decl>(genericTypeParamDecl, outerSubst)));
            }
            else if( auto genericValueParamDecl = as<GenericValueParamDecl>(mm) )
            {
                genericSubst->args.add(astBuilder->create<GenericParamIntVal>(DeclRef<GenericValueParamDecl>(genericValueParamDecl, outerSubst)));
            }
        }

        // create default substitution arguments for constraints
        for (auto mm : genericDecl->members)
        {
            if (auto genericTypeConstraintDecl = as<GenericTypeConstraintDecl>(mm))
            {
                DeclaredSubtypeWitness* witness = astBuilder->create<DeclaredSubtypeWitness>();
                witness->declRef = DeclRef<Decl>(genericTypeConstraintDecl, outerSubst);
                witness->sub = genericTypeConstraintDecl->sub.type;
                witness->sup = genericTypeConstraintDecl->sup.type;
                genericSubst->args.add(witness);
            }
        }

        return genericSubst;
    }

    // Sometimes we need to refer to a declaration the way that it would be specialized
    // inside the context where it is declared (e.g., with generic parameters filled in
    // using their archetypes).
    //
    SubstitutionSet createDefaultSubstitutions(
        ASTBuilder*     astBuilder, 
        Decl*           decl,
        SubstitutionSet outerSubstSet)
    {
        auto dd = decl->parentDecl;
        if( auto genericDecl = as<GenericDecl>(dd) )
        {
            // We don't want to specialize references to anything
            // other than the "inner" declaration itself.
            if(decl != genericDecl->inner)
                return outerSubstSet;

            GenericSubstitution* genericSubst = createDefaultSubstitutionsForGeneric(
                astBuilder,
                genericDecl,
                outerSubstSet.substitutions);

            return SubstitutionSet(genericSubst);
        }

        return outerSubstSet;
    }

    SubstitutionSet createDefaultSubstitutions(
        ASTBuilder* astBuilder, 
        Decl*   decl)
    {
        SubstitutionSet subst;
        if( auto parentDecl = decl->parentDecl )
        {
            subst = createDefaultSubstitutions(astBuilder, parentDecl);
        }
        subst = createDefaultSubstitutions(astBuilder, decl, subst);
        return subst;
    }

    void ensureDecl(SemanticsVisitor* visitor, Decl* decl, DeclCheckState state)
    {
        visitor->ensureDecl(decl, state);
    }

    bool SemanticsVisitor::isDeclUsableAsStaticMember(
        Decl*   decl)
    {
        if(auto genericDecl = as<GenericDecl>(decl))
            decl = genericDecl->inner;

        if(decl->hasModifier<HLSLStaticModifier>())
            return true;

        if(as<ConstructorDecl>(decl))
            return true;

        if(as<EnumCaseDecl>(decl))
            return true;

        if(as<AggTypeDeclBase>(decl))
            return true;

        if(as<SimpleTypeDecl>(decl))
            return true;

        if(as<TypeConstraintDecl>(decl))
            return true;

        return false;
    }

    bool SemanticsVisitor::isUsableAsStaticMember(
        LookupResultItem const& item)
    {
        // There's a bit of a gotcha here, because a lookup result
        // item might include "breadcrumbs" that indicate more steps
        // along the lookup path. As a result it isn't always
        // valid to just check whether the final decl is usable
        // as a static member, because it might not even be a
        // member of the thing we are trying to work with.
        //

        Decl* decl = item.declRef.getDecl();
        for(auto bb = item.breadcrumbs; bb; bb = bb->next)
        {
            switch(bb->kind)
            {
            // In case lookup went through a `__transparent` member,
            // we are interested in the static-ness of that transparent
            // member, and *not* the static-ness of whatever was inside
            // of it.
            //
            // TODO: This would need some work if we ever had
            // transparent *type* members.
            //
            case LookupResultItem::Breadcrumb::Kind::Member:
                decl = bb->declRef.getDecl();
                break;

            // TODO: Are there any other cases that need special-case
            // handling here?

            default:
                break;
            }
        }

        // Okay, we've found the declaration we should actually
        // be checking, so lets validate that.

        return isDeclUsableAsStaticMember(decl);
    }

        /// Dispatch an appropriate visitor to check `decl` up to state `state`
        ///
        /// The current state of `decl` must be `state-1`.
        /// This call does *not* handle updating the state of `decl`; the
        /// caller takes responsibility for doing so.
        ///
    static void _dispatchDeclCheckingVisitor(Decl* decl, DeclCheckState state, SharedSemanticsContext* shared);

    // Make sure a declaration has been checked, so we can refer to it.
    // Note that this may lead to us recursively invoking checking,
    // so this may not be the best way to handle things.
    void SemanticsVisitor::ensureDecl(Decl* decl, DeclCheckState state)
    {
        // If the `decl` has already been checked up to or beyond `state`
        // then there is nothing for us to do.
        //
        if (decl->isChecked(state)) return;

        // Is the declaration already being checked, somewhere up the
        // call stack from us?
        //
        if(decl->checkState.isBeingChecked())
        {
            // We tried to reference the same declaration while checking it!
            //
            // TODO: we should ideally be tracking a "chain" of declarations
            // being checked on the stack, so that we can report the full
            // chain that leads from this declaration back to itself.
            //
            getSink()->diagnose(decl, Diagnostics::cyclicReference, decl);
            return;
        }

        // Set the flag that indicates we are checking this declaration,
        // so that the cycle check above will catch us before we go
        // into any infinite loops.
        //
        decl->checkState.setIsBeingChecked(true);

        // Our task is to bring the `decl` up to `state` which may be
        // one or more steps ahead of where it currently is. We can
        // invoke a visitor designed to bring a declaration from state
        // N to state N+1, and in general we might need multiple such
        // passes to get `decl` to where we need it.
        //
        // The coding of this loop is somewhat defensive to deal
        // with special cases that will be described along the way.
        //
        for(;;)
        {
            // The first thing is to check what state the decl is
            // currently in at the start of this loop iteration,
            // and to bail out if it has been checked up to
            // (or beyond) our target state.
            //
            auto currentState = decl->checkState.getState();
            if(currentState >= state)
                break;

            // Because our visitors are only designed to go from state
            // N to N+1 in general, we will aspire to transition to
            // a state that is one greater than `currentState`.
            //
            auto nextState = DeclCheckState(Int(currentState) + 1);

            // We now dispatch an appropriate visitor based on `nextState`.
            //
            _dispatchDeclCheckingVisitor(decl, nextState, getShared());

            // In the common case, the visitor will have done the necessary
            // checking, but will *not* have updated the `checkState` on
            // `decl`. In that case we will do the update here, to save
            // us the complication of having to deal with state update in
            // every single visitor method.
            //
            // However, sometimes a visitor *will* want to manually update
            // the state of a declaration, and it may actually update it
            // *past* the `nextState` we asked for (or even past the
            // eventual target `state`). In those cases we don't want to
            // accidentally set the state of `decl` to something lower
            // than what has actually been checked, so we test for
            // such cases here.
            //
            if(nextState > decl->checkState.getState())
            {
                decl->setCheckState(nextState);
            }
        }

        // Once we are done here, the state of `decl` should have
        // been upgraded to (at least) `state`.
        //
        SLANG_ASSERT(decl->isChecked(state));

        // Now that we are done checking `decl` we need to restore
        // its "is being checked" flag so that we don't generate
        // errors the next time somebody calls `ensureDecl()` on it.
        //
        decl->checkState.setIsBeingChecked(false);
    }

        /// Recursively ensure the tree of declarations under `decl` is in `state`.
        ///
        /// This function does *not* handle declarations nested in function bodies
        /// because those cannot be meaningfully checked outside of the context
        /// of their surrounding statement(s).
        ///
    static void _ensureAllDeclsRec(
        SemanticsDeclVisitorBase*   visitor,
        Decl*                       decl,
        DeclCheckState              state)
    {
        // Ensure `decl` itself first.
        visitor->ensureDecl(decl, state);

        // If `decl` is a container, then we want to ensure its children.
        if(auto containerDecl = as<ContainerDecl>(decl))
        {
            // As an exception, if any of the child is a `ScopeDecl`,
            // then that indicates that it represents a scope for local
            // declarations under a statement (e.g., in a function body),
            // and we don't want to check such local declarations here.
            //
            for(auto childDecl : containerDecl->members)
            {
                if(as<ScopeDecl>(childDecl))
                    continue;

                _ensureAllDeclsRec(visitor, childDecl, state);
            }
        }

        // Note: the "inner" declaration of a `GenericDecl` is currently
        // not exposed as one of its children (despite a `GenericDecl`
        // being a `ContainerDecl`), so we need to handle the inner
        // declaration of a generic as another case here.
        //
        if(auto genericDecl = as<GenericDecl>(decl))
        {
            _ensureAllDeclsRec(visitor, genericDecl->inner, state);
        }
    }

    static bool isUnsizedArrayType(Type* type)
    {
        // Not an array?
        auto arrayType = as<ArrayExpressionType>(type);
        if (!arrayType) return false;

        // Explicit element count given?
        auto elementCount = arrayType->arrayLength;
        if (elementCount) return true;

        return true;
    }

    void SemanticsDeclHeaderVisitor::checkVarDeclCommon(VarDeclBase* varDecl)
    {
        // A variable that didn't have an explicit type written must
        // have its type inferred from the initial-value expression.
        //
        if(!varDecl->type.exp)
        {
            // In this case we need to perform all checking of the
            // variable (including semantic checking of the initial-value
            // expression) during the first phase of checking.

            auto initExpr = varDecl->initExpr;
            if(!initExpr)
            {
                getSink()->diagnose(varDecl, Diagnostics::varWithoutTypeMustHaveInitializer);
                varDecl->type.type = m_astBuilder->getErrorType();
            }
            else
            {
                initExpr = CheckExpr(initExpr);

                // TODO: We might need some additional steps here to ensure
                // that the type of the expression is one we are okay with
                // inferring. E.g., if we ever decide that integer and floating-point
                // literals have a distinct type from the standard int/float types,
                // then we would need to "decay" a literal to an explicit type here.

                varDecl->initExpr = initExpr;
                varDecl->type.type = initExpr->type;
            }

            // If we've gone down this path, then the variable
            // declaration is actually pretty far along in checking
            varDecl->setCheckState(DeclCheckState::Checked);
        }
        else
        {
            // A variable with an explicit type is simpler, for the
            // most part.

            TypeExp typeExp = CheckUsableType(varDecl->type);
            varDecl->type = typeExp;
            if (varDecl->type.equals(m_astBuilder->getVoidType()))
            {
                getSink()->diagnose(varDecl, Diagnostics::invalidTypeVoid);
            }

            // If this is an unsized array variable, then we first want to give
            // it a chance to infer an array size from its initializer
            //
            // TODO(tfoley): May need to extend this to handle the
            // multi-dimensional case...
            //
            if(isUnsizedArrayType(varDecl->type))
            {
                if (auto initExpr = varDecl->initExpr)
                {
                    initExpr = CheckTerm(initExpr);
                    initExpr = coerce(varDecl->type.Ptr(), initExpr);
                    varDecl->initExpr = initExpr;

                    maybeInferArraySizeForVariable(varDecl);

                    varDecl->setCheckState(DeclCheckState::Checked);
                }
            }
            //
            // Next we want to make sure that the declared (or inferred)
            // size for the array meets whatever language-specific
            // constraints we want to enforce (e.g., disallow empty
            // arrays in specific cases)
            //
            validateArraySizeForVariable(varDecl);
        }
    }

    void SemanticsDeclBodyVisitor::checkVarDeclCommon(VarDeclBase* varDecl)
    {
        if (auto initExpr = varDecl->initExpr)
        {
            // If the variable has an explicit initial-value expression,
            // then we simply need to check that expression and coerce
            // it to the tyep of the variable.
            //
            initExpr = CheckTerm(initExpr);
            initExpr = coerce(varDecl->type.Ptr(), initExpr);
            varDecl->initExpr = initExpr;
        }
        else
        {
            // If a variable doesn't have an explicit initial-value
            // expression, it is still possible that it should
            // be initialized implicitly, because the type of the
            // variable has a default (zero parameter) initializer.
            // That is, for types where it is possible, we will
            // treat a variable declared like this:
            //
            //      MyType myVar;
            //
            // as if it were declared as:
            //
            //      MyType myVar = MyType();
            //
            // Rather than try to code up an ad hoc search for an
            // appropriate initializer here, we will instead fall
            // back on the general-purpose overload-resolution
            // machinery, which can handle looking up initializers
            // and filtering them to ones that are applicable
            // to our "call site" with zero arguments.
            //
            auto type = varDecl->getType();

            OverloadResolveContext overloadContext;
            overloadContext.loc = varDecl->nameAndLoc.loc;
            overloadContext.mode = OverloadResolveContext::Mode::JustTrying;
            AddTypeOverloadCandidates(type, overloadContext);

            if(overloadContext.bestCandidates.getCount() != 0)
            {
                // If there were multiple equally-good candidates to call,
                // then might have an ambiguity.
                //
                // Before issuing any kind of diagnostic we need to check
                // if any of those candidates are actually applicable,
                // because if they aren't then we actually just have
                // an uninitialized varaible.
                //
                if(overloadContext.bestCandidates[0].status != OverloadCandidate::Status::Applicable)
                    return;

                getSink()->diagnose(varDecl, Diagnostics::ambiguousDefaultInitializerForType, type);
            }
            else if(overloadContext.bestCandidate)
            {
                // If we are in the single-candidate case, then we again
                // want to ignore the case where that candidate wasn't
                // actually applicable, because declaring a variable
                // of a type that *doesn't* have a default initializer
                // isn't actually an error.
                //
                if(overloadContext.bestCandidate->status != OverloadCandidate::Status::Applicable)
                    return;

                // If we had a single best candidate *and* it was applicable,
                // then we use it to construct a new initial-value expression
                // for the variable, that will be used for all downstream
                // code generation.
                //
                varDecl->initExpr = CompleteOverloadCandidate(overloadContext, *overloadContext.bestCandidate);
            }
        }
    }

    // Fill in default substitutions for the 'subtype' part of a type constraint decl
    void SemanticsVisitor::CheckConstraintSubType(TypeExp& typeExp)
    {
        if (auto sharedTypeExpr = as<SharedTypeExpr>(typeExp.exp))
        {
            if (auto declRefType = as<DeclRefType>(sharedTypeExpr->base))
            {
                declRefType->declRef.substitutions = createDefaultSubstitutions(m_astBuilder, declRefType->declRef.getDecl());

                if (auto typetype = as<TypeType>(typeExp.exp->type))
                    typetype->type = declRefType;
            }
        }
    }

    void SemanticsDeclHeaderVisitor::visitGenericTypeConstraintDecl(GenericTypeConstraintDecl* decl)
    {
        // TODO: are there any other validations we can do at this point?
        //
        // There probably needs to be a kind of "occurs check" to make
        // sure that the constraint actually applies to at least one
        // of the parameters of the generic.
        //
        CheckConstraintSubType(decl->sub);
        decl->sub = TranslateTypeNodeForced(decl->sub);
        decl->sup = TranslateTypeNodeForced(decl->sup);
    }

    void SemanticsDeclHeaderVisitor::visitGenericTypeParamDecl(GenericTypeParamDecl* decl)
    {
        // TODO: could probably push checking the default value
        // for a generic type parameter later.
        //
        decl->initType = CheckProperType(decl->initType);
    }

    void SemanticsDeclHeaderVisitor::visitGenericValueParamDecl(GenericValueParamDecl* decl)
    {
        checkVarDeclCommon(decl);
    }

    void SemanticsDeclHeaderVisitor::visitGenericDecl(GenericDecl* genericDecl)
    {
        genericDecl->setCheckState(DeclCheckState::ReadyForLookup);

        for (auto m : genericDecl->members)
        {
            if (auto typeParam = as<GenericTypeParamDecl>(m))
            {
                ensureDecl(typeParam, DeclCheckState::ReadyForReference);
            }
            else if (auto valParam = as<GenericValueParamDecl>(m))
            {
                ensureDecl(valParam, DeclCheckState::ReadyForReference);
            }
            else if (auto constraint = as<GenericTypeConstraintDecl>(m))
            {
                ensureDecl(constraint, DeclCheckState::ReadyForReference);
            }
        }
    }

    void SemanticsDeclBasesVisitor::visitInheritanceDecl(InheritanceDecl* inheritanceDecl)
    {
        // check the type being inherited from
        auto base = inheritanceDecl->base;
        CheckConstraintSubType(base);
        base = TranslateTypeNode(base);
        inheritanceDecl->base = base;

        // For now we only allow inheritance from interfaces, so
        // we will validate that the type expression names an interface

        if(auto declRefType = as<DeclRefType>(base.type))
        {
            if(auto interfaceDeclRef = declRefType->declRef.as<InterfaceDecl>())
            {
                return;
            }
        }
        else if(as<ErrorType>(base.type))
        {
            // If an error was already produced, don't emit a cascading error.
            return;
        }

        // If type expression didn't name an interface, we'll emit an error here
        // TODO: deal with the case of an error in the type expression (don't cascade)
        getSink()->diagnose( base.exp, Diagnostics::expectedAnInterfaceGot, base.type);
    }

        // Concretize interface conformances so that we have witnesses as required for lookup.
        // for lookup.
    struct SemanticsDeclConformancesVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclConformancesVisitor>
    {
        SemanticsDeclConformancesVisitor(SharedSemanticsContext* shared)
            : SemanticsDeclVisitorBase(shared)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        // Any user-defined type may have declared interface conformances,
        // which we should check.
        //
        void visitAggTypeDecl(AggTypeDecl* aggTypeDecl)
        {
            checkAggTypeConformance(aggTypeDecl);
        }

        // Conformances can also come via `extension` declarations, and
        // we should check them against the type(s) being extended.
        //
        void visitExtensionDecl(ExtensionDecl* extensionDecl)
        {
            checkExtensionConformance(extensionDecl);
        }
    };

        /// Recursively register any builtin declarations that need to be attached to the `session`.
        ///
        /// This function should only be needed for declarations in the standard library.
        ///
    static void _registerBuiltinDeclsRec(Session* session, Decl* decl)
    {
        SharedASTBuilder* sharedASTBuilder = session->m_sharedASTBuilder;

        if (auto builtinMod = decl->findModifier<BuiltinTypeModifier>())
        {
            sharedASTBuilder->registerBuiltinDecl(decl, builtinMod);
        }
        if (auto magicMod = decl->findModifier<MagicTypeModifier>())
        {
            sharedASTBuilder->registerMagicDecl(decl, magicMod);
        }

        if(auto containerDecl = as<ContainerDecl>(decl))
        {
            for(auto childDecl : containerDecl->members)
            {
                if(as<ScopeDecl>(childDecl))
                    continue;

                _registerBuiltinDeclsRec(session, childDecl);
            }
        }
        if(auto genericDecl = as<GenericDecl>(decl))
        {
            _registerBuiltinDeclsRec(session, genericDecl->inner);
        }
    }

    void SemanticsDeclVisitorBase::checkModule(ModuleDecl* moduleDecl)
    {
        // When we are dealing with code from the standard library,
        // there is a potential problem where we might need to look
        // up built-in types like `Int` through the session (e.g.,
        // to determine the type for an integer literal), but those
        // types might not have been registered yet. We solve that
        // by doing a pre-process on standard-library code to find
        // and register any built-in declarations.
        //
        // TODO: This could be factored into another visitor pass
        // that fits the more standard checking below, but that would
        // seemingly add overhead to checking things other than
        // the standard library.
        //
        if(isFromStdLib(moduleDecl))
        {
            _registerBuiltinDeclsRec(getSession(), moduleDecl);
        }

        // We need/want to visit any `import` declarations before
        // anything else, to make sure that scoping works.
        //
        // TODO: This could be factored into another visitor pass
        // that fits more with the standard checking below.
        //
        for(auto& importDecl : moduleDecl->getMembersOfType<ImportDecl>())
        {
            ensureDecl(importDecl, DeclCheckState::Checked);
        }

        // The entire goal of semantic checking is to get all of the
        // declarations in the module up to `DeclCheckState::Checked`.
        //
        // The main catch is that checking one declaration A up to state M
        // may required that declaration B is checked up to state N.
        // A call to `ensureDecl(B, N)` can guarantee that things are checked
        // when and where we need them, but that runs the risk of creating
        // very deep recursion in the semantic checking.
        //
        // Instead, we would rather do more breadth-first checking,
        // where everything gets checked up to state 1, 2, ...
        // before anything gets too far ahead.
        // We will therefore enumerate the states/phases for checking,
        // and then iteratively try to update all declarations to each
        // state in turn.
        //
        // Note: for a simpler language we could eliminate `ensureDecl`
        // completely and *just* have these phases of checking.
        // Unfortunately, we have some circularity between the phases:
        //
        // * Checking an overloaded call requires knowing the parameter
        //   types of all candidate callees.
        //
        // * Checking the parameter type of a function requires being
        //   able to check type expressions.
        //
        // * A type expression like `vector<T, N>` may have an arbitary
        //   expression for `N`.
        //
        // * An arbitrary expression may include function calls, which
        //   may be to overloaded functions.
        //
        // Languages like C++ solve the apparent problem by making
        // restrictions on order of declaration/definition (and by
        // requiring forward declarations or the `template`/`typename`
        // keywrods in some cases).
        //
        // TODO: We could eventually eliminate the potential recursion
        // in checking by splitting each phase into a "requirements gathering"
        // step and an actual execution step.
        //
        // When checking a declaration D up to state S, the requirements
        // gathering step would produce a list of pairs `(someDecl, someState)`
        // indicating that `someDecl` must be in `someState` before the
        // actual execution of checking for `(D,S)` can proceeed. The checker
        // can then produce an elaborated dependency graph and select nodes
        // for execution in an order that satisfies all the dependencies.
        //
        // Such a more elaborate checking scheme will have to wait for another
        // day, but might be worth it (or even necessary) if/when we want to
        // support incremental compilation.
        //
        DeclCheckState states[] =
        {
            DeclCheckState::ModifiersChecked,
            DeclCheckState::ReadyForReference,
            DeclCheckState::ReadyForLookup,
            DeclCheckState::ReadyForLookup,
            DeclCheckState::Checked
        };
        for(auto s : states)
        {
            // When advancing to state `s` we will recursively
            // advance all declarations rooted in the module
            // up to `s`.
            //
            // TODO: In cases where a large module is split across files,
            // we could potentially parallelize front-end compilation by
            // having multiple instances of the front end where each is
            // only responsible for those declarations in a given file.
            //
            // Under that model, we might only apply later phases of
            // checking (notably the final push to `DeclState::Checked`)
            // to the subset of declarations coming from a given source
            // file.
            //
            _ensureAllDeclsRec(this, moduleDecl, s);
        }

        // Once we have completed the above loop, all declarations not
        // nested in function bodies should be in `DeclState::Checked`.
        // Furthermore, because a fully checked function will have checked
        // its body, this also means that all function bodies and the
        // declarations they contain should be fully checked.
    }

    bool SemanticsVisitor::doesSignatureMatchRequirement(
        DeclRef<CallableDecl>   satisfyingMemberDeclRef,
        DeclRef<CallableDecl>   requiredMemberDeclRef,
        RefPtr<WitnessTable>    witnessTable)
    {
        if(satisfyingMemberDeclRef.getDecl()->hasModifier<MutatingAttribute>()
            && !requiredMemberDeclRef.getDecl()->hasModifier<MutatingAttribute>())
        {
            // A `[mutating]` method can't satisfy a non-`[mutating]` requirement,
            // but vice-versa is okay.
            return false;
        }

        if(satisfyingMemberDeclRef.getDecl()->hasModifier<HLSLStaticModifier>()
            != requiredMemberDeclRef.getDecl()->hasModifier<HLSLStaticModifier>())
        {
            // A `static` method can't satisfy a non-`static` requirement and vice versa.
            return false;
        }

        // TODO: actually implement matching here. For now we'll
        // just pretend that things are satisfied in order to make progress..
        witnessTable->requirementDictionary.Add(
            requiredMemberDeclRef.getDecl(),
            RequirementWitness(satisfyingMemberDeclRef));
        return true;
    }

    bool SemanticsVisitor::doesGenericSignatureMatchRequirement(
        DeclRef<GenericDecl>        genDecl,
        DeclRef<GenericDecl>        requirementGenDecl,
        RefPtr<WitnessTable>        witnessTable)
    {
        if (genDecl.getDecl()->members.getCount() != requirementGenDecl.getDecl()->members.getCount())
            return false;
        for (Index i = 0; i < genDecl.getDecl()->members.getCount(); i++)
        {
            auto genMbr = genDecl.getDecl()->members[i];
            auto requiredGenMbr = genDecl.getDecl()->members[i];
            if (auto genTypeMbr = as<GenericTypeParamDecl>(genMbr))
            {
                if (auto requiredGenTypeMbr = as<GenericTypeParamDecl>(requiredGenMbr))
                {
                }
                else
                    return false;
            }
            else if (auto genValMbr = as<GenericValueParamDecl>(genMbr))
            {
                if (auto requiredGenValMbr = as<GenericValueParamDecl>(requiredGenMbr))
                {
                    if (!genValMbr->type->equals(requiredGenValMbr->type))
                        return false;
                }
                else
                    return false;
            }
            else if (auto genTypeConstraintMbr = as<GenericTypeConstraintDecl>(genMbr))
            {
                if (auto requiredTypeConstraintMbr = as<GenericTypeConstraintDecl>(requiredGenMbr))
                {
                    if (!genTypeConstraintMbr->sup->equals(requiredTypeConstraintMbr->sup))
                    {
                        return false;
                    }
                }
                else
                    return false;
            }
        }

        // TODO: this isn't right, because we need to specialize the
        // declarations of the generics to a common set of substitutions,
        // so that their types are comparable (e.g., foo<T> and foo<U>
        // need to have substitutions applies so that they are both foo<X>,
        // after which uses of the type X in their parameter lists can
        // be compared).

        return doesMemberSatisfyRequirement(
            DeclRef<Decl>(genDecl.getDecl()->inner, genDecl.substitutions),
            DeclRef<Decl>(requirementGenDecl.getDecl()->inner, requirementGenDecl.substitutions),
            witnessTable);
    }

    bool SemanticsVisitor::doesTypeSatisfyAssociatedTypeRequirement(
        Type*            satisfyingType,
        DeclRef<AssocTypeDecl>  requiredAssociatedTypeDeclRef,
        RefPtr<WitnessTable>    witnessTable)
    {
        // We need to confirm that the chosen type `satisfyingType`,
        // meets all the constraints placed on the associated type
        // requirement `requiredAssociatedTypeDeclRef`.
        //
        // We will enumerate the type constraints placed on the
        // associated type and see if they can be satisfied.
        //
        bool conformance = true;
        for (auto requiredConstraintDeclRef : getMembersOfType<TypeConstraintDecl>(requiredAssociatedTypeDeclRef))
        {
            // Grab the type we expect to conform to from the constraint.
            auto requiredSuperType = getSup(m_astBuilder, requiredConstraintDeclRef);

            // Perform a search for a witness to the subtype relationship.
            auto witness = tryGetSubtypeWitness(satisfyingType, requiredSuperType);
            if(witness)
            {
                // If a subtype witness was found, then the conformance
                // appears to hold, and we can satisfy that requirement.
                witnessTable->requirementDictionary.Add(requiredConstraintDeclRef, RequirementWitness(witness));
            }
            else
            {
                // If a witness couldn't be found, then the conformance
                // seems like it will fail.
                conformance = false;
            }
        }

        // TODO: if any conformance check failed, we should probably include
        // that in an error message produced about not satisfying the requirement.

        if(conformance)
        {
            // If all the constraints were satisfied, then the chosen
            // type can indeed satisfy the interface requirement.
            witnessTable->requirementDictionary.Add(
                requiredAssociatedTypeDeclRef.getDecl(),
                RequirementWitness(satisfyingType));
        }

        return conformance;
    }

    bool SemanticsVisitor::doesMemberSatisfyRequirement(
        DeclRef<Decl>               memberDeclRef,
        DeclRef<Decl>               requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        // Sanity check: if are checking whether a type `T`
        // implements, say, `IFoo::bar` and lookup of `bar`
        // in type `T` yielded `IFoo::bar`, then that shouldn't
        // be treated as a valid satisfaction of the requirement.
        //
        // TODO: Ideally this check should be comparing the `DeclRef`s
        // and not just the `Decl`s, but we currently don't get exactly
        // the same substitutions when we see the inherited `IFoo::bar`.
        //
        if(memberDeclRef.getDecl() == requiredMemberDeclRef.getDecl())
            return false;

        // At a high level, we want to check that the
        // `memberDecl` and the `requiredMemberDeclRef`
        // have the same AST node class, and then also
        // check that their signatures match.
        //
        // There are a bunch of detailed decisions that
        // have to be made, though, because we might, e.g.,
        // allow a function with more general parameter
        // types to satisfy a requirement with more
        // specific parameter types.
        //
        // If we ever allow for "property" declarations,
        // then we would probably need to allow an
        // ordinary field to satisfy a property requirement.
        //
        // An associated type requirement should be allowed
        // to be satisfied by any type declaration:
        // a typedef, a `struct`, etc.
        //
        if (auto memberFuncDecl = memberDeclRef.as<FuncDecl>())
        {
            if (auto requiredFuncDeclRef = requiredMemberDeclRef.as<FuncDecl>())
            {
                // Check signature match.
                return doesSignatureMatchRequirement(
                    memberFuncDecl,
                    requiredFuncDeclRef,
                    witnessTable);
            }
        }
        else if (auto memberInitDecl = memberDeclRef.as<ConstructorDecl>())
        {
            if (auto requiredInitDecl = requiredMemberDeclRef.as<ConstructorDecl>())
            {
                // Check signature match.
                return doesSignatureMatchRequirement(
                    memberInitDecl,
                    requiredInitDecl,
                    witnessTable);
            }
        }
        else if (auto genDecl = memberDeclRef.as<GenericDecl>())
        {
            // For a generic member, we will check if it can satisfy
            // a generic requirement in the interface.
            //
            // TODO: we could also conceivably check that the generic
            // could be *specialized* to satisfy the requirement,
            // and then install a specialization of the generic into
            // the witness table. Actually doing this would seem
            // to require performing something akin to overload
            // resolution as part of requirement satisfaction.
            //
            if (auto requiredGenDeclRef = requiredMemberDeclRef.as<GenericDecl>())
            {
                return doesGenericSignatureMatchRequirement(genDecl, requiredGenDeclRef, witnessTable);
            }
        }
        else if (auto subAggTypeDeclRef = memberDeclRef.as<AggTypeDecl>())
        {
            if(auto requiredTypeDeclRef = requiredMemberDeclRef.as<AssocTypeDecl>())
            {
                ensureDecl(subAggTypeDeclRef, DeclCheckState::CanUseAsType);

                auto satisfyingType = DeclRefType::create(m_astBuilder, subAggTypeDeclRef);
                return doesTypeSatisfyAssociatedTypeRequirement(satisfyingType, requiredTypeDeclRef, witnessTable);
            }
        }
        else if (auto typedefDeclRef = memberDeclRef.as<TypeDefDecl>())
        {
            // this is a type-def decl in an aggregate type
            // check if the specified type satisfies the constraints defined by the associated type
            if (auto requiredTypeDeclRef = requiredMemberDeclRef.as<AssocTypeDecl>())
            {
                ensureDecl(typedefDeclRef, DeclCheckState::CanUseAsType);

                auto satisfyingType = getNamedType(m_astBuilder, typedefDeclRef);
                return doesTypeSatisfyAssociatedTypeRequirement(satisfyingType, requiredTypeDeclRef, witnessTable);
            }
        }
        // Default: just assume that thing aren't being satisfied.
        return false;
    }

    bool SemanticsVisitor::findWitnessForInterfaceRequirement(
        ConformanceCheckingContext* context,
        Type*                       type,
        InheritanceDecl*            inheritanceDecl,
        DeclRef<InterfaceDecl>      interfaceDeclRef,
        DeclRef<Decl>               requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        SLANG_UNUSED(interfaceDeclRef)

        // The goal of this function is to find a suitable
        // value to satisfy the requirement.
        //
        // The 99% case is that the requirement is a named member
        // of the interface, and we need to search for a member
        // with the same name in the type declaration and
        // its (known) extensions.

        // As a first pass, lets check if we already have a
        // witness in the table for the requirement, so
        // that we can bail out early.
        //
        if(witnessTable->requirementDictionary.ContainsKey(requiredMemberDeclRef.getDecl()))
        {
            return true;
        }


        // An important exception to the above is that an
        // inheritance declaration in the interface is not going
        // to be satisfied by an inheritance declaration in the
        // conforming type, but rather by a full "witness table"
        // full of the satisfying values for each requirement
        // in the inherited-from interface.
        //
        if( auto requiredInheritanceDeclRef = requiredMemberDeclRef.as<InheritanceDecl>() )
        {
            // Recursively check that the type conforms
            // to the inherited interface.
            //
            // TODO: we *really* need a linearization step here!!!!

            RefPtr<WitnessTable> satisfyingWitnessTable = checkConformanceToType(
                context,
                type,
                requiredInheritanceDeclRef.getDecl(),
                getBaseType(m_astBuilder, requiredInheritanceDeclRef));

            if(!satisfyingWitnessTable)
                return false;

            witnessTable->requirementDictionary.Add(
                requiredInheritanceDeclRef.getDecl(),
                RequirementWitness(satisfyingWitnessTable));
            return true;
        }

        // We will look up members with the same name,
        // since only same-name members will be able to
        // satisfy the requirement.
        //
        Name* name = requiredMemberDeclRef.getName();

        // We start by looking up members of the same
        // name, on the type that is claiming to conform.
        //
        // This lookup step could include members that
        // we might not actually want to consider:
        //
        // * Lookup through a type `Foo` where `Foo : IBar`
        //   will be able to find members of `IBar`, which
        //   somewhat obviously shouldn't apply when
        //   determining if `Foo` satisfies the requirements
        //   of `IBar`.
        //
        // * Lookup in the presence of `__transparent` members
        //   may produce references to declarations on a *field*
        //   of the type rather than the type. Conformance through
        //   transparent members could be supported in theory,
        //   but would require synthesizing proxy/forwarding
        //   implementations in the type itself.
        //
        // We will punt on the second issue for now (since
        // transparent members aren't currently exposed as
        // a general-purpose feature for users), and rely
        // on subsequent checking in this function to
        // rule out inherited abstract members.
        //
        auto lookupResult = lookUpMember(m_astBuilder, this, name, type);

        // Iterate over the members and look for one that matches
        // the expected signature for the requirement.
        for (auto member : lookupResult)
        {
            if (doesMemberSatisfyRequirement(member.declRef, requiredMemberDeclRef, witnessTable))
                return true;
        }

        // No suitable member found, although there were candidates.
        //
        // TODO: Eventually we might want something akin to the current
        // overload resolution logic, where we keep track of a list
        // of "candidates" for satisfaction of the requirement,
        // and if nothing is found we print the candidates

        getSink()->diagnose(inheritanceDecl, Diagnostics::typeDoesntImplementInterfaceRequirement, type, requiredMemberDeclRef);
        return false;
    }

    RefPtr<WitnessTable> SemanticsVisitor::checkInterfaceConformance(
        ConformanceCheckingContext* context,
        Type*                       type,
        InheritanceDecl*            inheritanceDecl,
        DeclRef<InterfaceDecl>      interfaceDeclRef)
    {
        // Has somebody already checked this conformance,
        // and/or is in the middle of checking it?
        RefPtr<WitnessTable> witnessTable;
        if(context->mapInterfaceToWitnessTable.TryGetValue(interfaceDeclRef, witnessTable))
            return witnessTable;

        // We need to check the declaration of the interface
        // before we can check that we conform to it.
        //
        ensureDecl(interfaceDeclRef, DeclCheckState::CanReadInterfaceRequirements);

        // We will construct the witness table, and register it
        // *before* we go about checking fine-grained requirements,
        // in order to short-circuit any potential for infinite recursion.

        // Note: we will re-use the witnes table attached to the inheritance decl,
        // if there is one. This catches cases where semantic checking might
        // have synthesized some of the conformance witnesses for us.
        //
        witnessTable = inheritanceDecl->witnessTable;
        if(!witnessTable)
        {
            witnessTable = new WitnessTable();
        }
        context->mapInterfaceToWitnessTable.Add(interfaceDeclRef, witnessTable);

        bool result = true;

        // TODO: If we ever allow for implementation inheritance,
        // then we will need to consider the case where a type
        // declares that it conforms to an interface, but one of
        // its (non-interface) base types already conforms to
        // that interface, so that all of the requirements are
        // already satisfied with inherited implementations...
        for(auto requiredMemberDeclRef : getMembers(interfaceDeclRef))
        {
            auto requirementSatisfied = findWitnessForInterfaceRequirement(
                context,
                type,
                inheritanceDecl,
                interfaceDeclRef,
                requiredMemberDeclRef,
                witnessTable);

            result = result && requirementSatisfied;
        }

        // Extensions that apply to the interface type can create new conformances
        // for the concrete types that inherit from the interface.
        //
        // These new conformances should not be able to introduce new *requirements*
        // for an implementing interface (although they currently can), but we
        // still need to go through this logic to find the appropriate value
        // that will satisfy the requirement in these cases, and also to put
        // the required entry into the witness table for the interface itself.
        //
        // TODO: This logic is a bit slippery, and we need to figure out what
        // it means in the context of separate compilation. If module A defines
        // an interface IA, module B defines a type C that conforms to IA, and then
        // module C defines an extension that makes IA conform to IC, then it is
        // unreasonable to expect the {B:IA} witness table to contain an entry
        // corresponding to {IA:IC}.
        //
        // The simple answer then would be that the {IA:IC} conformance should be
        // fixed, with a single witness table for {IA:IC}, but then what should
        // happen in B explicitly conformed to IC already?
        //
        // For now we will just walk through the extensions that are known at
        // the time we are compiling and handle those, and punt on the larger issue
        // for abit longer.
        for(auto candidateExt = interfaceDeclRef.getDecl()->candidateExtensions; candidateExt; candidateExt = candidateExt->nextCandidateExtension)
        {
            // We need to apply the extension to the interface type that our
            // concrete type is inheriting from.
            //
            // TODO: need to decide if a this-type substitution is needed here.
            // It probably it.
            Type* targetType = DeclRefType::create(m_astBuilder, interfaceDeclRef);
            auto extDeclRef = ApplyExtensionToType(candidateExt, targetType);
            if(!extDeclRef)
                continue;

            // Only inheritance clauses from the extension matter right now.
            for(auto requiredInheritanceDeclRef : getMembersOfType<InheritanceDecl>(extDeclRef))
            {
                auto requirementSatisfied = findWitnessForInterfaceRequirement(
                    context,
                    type,
                    inheritanceDecl,
                    interfaceDeclRef,
                    requiredInheritanceDeclRef,
                    witnessTable);

                result = result && requirementSatisfied;
            }
        }

        // If we failed to satisfy any requirements along the way,
        // then we don't actually want to keep the witness table
        // we've been constructing, because the whole thing was a failure.
        if(!result)
        {
            return nullptr;
        }

        return witnessTable;
    }

    RefPtr<WitnessTable> SemanticsVisitor::checkConformanceToType(
        ConformanceCheckingContext* context,
        Type*                       type,
        InheritanceDecl*            inheritanceDecl,
        Type*                       baseType)
    {
        if (auto baseDeclRefType = as<DeclRefType>(baseType))
        {
            auto baseTypeDeclRef = baseDeclRefType->declRef;
            if (auto baseInterfaceDeclRef = baseTypeDeclRef.as<InterfaceDecl>())
            {
                // The type is stating that it conforms to an interface.
                // We need to check that it provides all of the members
                // required by that interface.
                return checkInterfaceConformance(
                    context,
                    type,
                    inheritanceDecl,
                    baseInterfaceDeclRef);
            }
        }

        getSink()->diagnose(inheritanceDecl, Diagnostics::unimplemented, "type not supported for inheritance");
        return nullptr;
    }

    bool SemanticsVisitor::checkConformance(
        Type*                       type,
        InheritanceDecl*            inheritanceDecl)
    {
        if( auto declRefType = as<DeclRefType>(type) )
        {
            auto declRef = declRefType->declRef;

            // Don't check conformances for abstract types that
            // are being used to express *required* conformances.
            if (auto assocTypeDeclRef = declRef.as<AssocTypeDecl>())
            {
                // An associated type declaration represents a requirement
                // in an outer interface declaration, and its members
                // (type constraints) represent additional requirements.
                return true;
            }
            else if (auto interfaceDeclRef = declRef.as<InterfaceDecl>())
            {
                // HACK: Our semantics as they stand today are that an
                // `extension` of an interface that adds a new inheritance
                // clause acts *as if* that inheritnace clause had been
                // attached to the original `interface` decl: that is,
                // it adds additional requirements.
                //
                // This is *not* a reasonable semantic to keep long-term,
                // but it is required for some of our current example
                // code to work.
                return true;
            }
        }

        // Look at the type being inherited from, and validate
        // appropriately.
        auto baseType = inheritanceDecl->base.type;

        ConformanceCheckingContext context;
        RefPtr<WitnessTable> witnessTable = checkConformanceToType(&context, type, inheritanceDecl, baseType);
        if(!witnessTable)
            return false;

        inheritanceDecl->witnessTable = witnessTable;
        return true;
    }

    void SemanticsVisitor::checkExtensionConformance(ExtensionDecl* decl)
    {
        auto declRef = createDefaultSubstitutionsIfNeeded(m_astBuilder, makeDeclRef(decl)).as<ExtensionDecl>();
        auto targetType = getTargetType(m_astBuilder, declRef);

        for (auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
        {
            checkConformance(targetType, inheritanceDecl);
        }
    }

    void SemanticsVisitor::checkAggTypeConformance(AggTypeDecl* decl)
    {
        // After we've checked members, we need to go through
        // any inheritance clauses on the type itself, and
        // confirm that the type actually provides whatever
        // those clauses require.

        if (auto interfaceDecl = as<InterfaceDecl>(decl))
        {
            // Don't check that an interface conforms to the
            // things it inherits from.
        }
        else if (auto assocTypeDecl = as<AssocTypeDecl>(decl))
        {
            // Don't check that an associated type decl conforms to the
            // things it inherits from.
        }
        else
        {
            // For non-interface types we need to check conformance.
            //

            auto astBuilder = getASTBuilder();

            auto declRef = createDefaultSubstitutionsIfNeeded(astBuilder, makeDeclRef(decl)).as<AggTypeDeclBase>();
            auto type = DeclRefType::create(astBuilder, declRef);

            // TODO: Need to figure out what this should do for
            // `abstract` types if we ever add them. Should they
            // be required to implement all interface requirements,
            // just with `abstract` methods that replicate things?
            // (That's what C# does).
            for (auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
            {
                checkConformance(type, inheritanceDecl);
            }
        }
    }

    void SemanticsDeclBasesVisitor::visitAggTypeDecl(AggTypeDecl* decl)
    {
        // TODO: We need to enumerate the bases here,
        // and ideally form a "class precedence list"
        // from them.

        for( auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>() )
        {
            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);
        }
    }

    bool SemanticsVisitor::isIntegerBaseType(BaseType baseType)
    {
        return (BaseTypeInfo::getInfo(baseType).flags & BaseTypeInfo::Flag::Integer) != 0;
    }

    void SemanticsVisitor::validateEnumTagType(Type* type, SourceLoc const& loc)
    {
        if(auto basicType = as<BasicExpressionType>(type))
        {
            // Allow the built-in integer types.
            if(isIntegerBaseType(basicType->baseType))
                return;

            // By default, don't allow other types to be used
            // as an `enum` tag type.
        }

        getSink()->diagnose(loc, Diagnostics::invalidEnumTagType, type);
    }

    void SemanticsDeclBasesVisitor::visitEnumDecl(EnumDecl* decl)
    {
        // Look at inheritance clauses, and
        // see if one of them is making the enum
        // "inherit" from a concrete type.
        // This will become the "tag" type
        // of the enum.
        Type* tagType = nullptr;
        InheritanceDecl*    tagTypeInheritanceDecl = nullptr;
        for(auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
        {
            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);

            // Look at the type being inherited from.
            auto superType = inheritanceDecl->base.type;

            if(auto errorType = as<ErrorType>(superType))
            {
                // Ignore any erroneous inheritance clauses.
                continue;
            }
            else if(auto declRefType = as<DeclRefType>(superType))
            {
                if(auto interfaceDeclRef = declRefType->declRef.as<InterfaceDecl>())
                {
                    // Don't consider interface bases as candidates for
                    // the tag type.
                    continue;
                }
            }

            if(tagType)
            {
                // We already found a tag type.
                getSink()->diagnose(inheritanceDecl, Diagnostics::enumTypeAlreadyHasTagType);
                getSink()->diagnose(tagTypeInheritanceDecl, Diagnostics::seePreviousTagType);
                break;
            }
            else
            {
                tagType = superType;
                tagTypeInheritanceDecl = inheritanceDecl;
            }
        }

        // If a tag type has not been set, then we
        // default it to the built-in `int` type.
        //
        // TODO: In the far-flung future we may want to distinguish
        // `enum` types that have a "raw representation" like this from
        // ones that are purely abstract and don't expose their
        // type of their tag.
        if(!tagType)
        {
            tagType = m_astBuilder->getIntType();
        }
        else
        {
            // TODO: Need to establish that the tag
            // type is suitable. (e.g., if we are going
            // to allow raw values for case tags to be
            // derived automatically, then the tag
            // type needs to be some kind of integer type...)
            //
            // For now we will just be harsh and require it
            // to be one of a few builtin types.
            validateEnumTagType(tagType, tagTypeInheritanceDecl->loc);
        }
        decl->tagType = tagType;


        // An `enum` type should automatically conform to the `__EnumType` interface.
        // The compiler needs to insert this conformance behind the scenes, and this
        // seems like the best place to do it.
        {
            // First, look up the type of the `__EnumType` interface.
            Type* enumTypeType = getASTBuilder()->getEnumTypeType();

            InheritanceDecl* enumConformanceDecl = m_astBuilder->create<InheritanceDecl>();
            enumConformanceDecl->parentDecl = decl;
            enumConformanceDecl->loc = decl->loc;
            enumConformanceDecl->base.type = getASTBuilder()->getEnumTypeType();
            decl->members.add(enumConformanceDecl);

            // The `__EnumType` interface has one required member, the `__Tag` type.
            // We need to satisfy this requirement automatically, rather than require
            // the user to actually declare a member with this name (otherwise we wouldn't
            // let them define a tag value with the name `__Tag`).
            //
            RefPtr<WitnessTable> witnessTable = new WitnessTable();
            enumConformanceDecl->witnessTable = witnessTable;

            Name* tagAssociatedTypeName = getSession()->getNameObj("__Tag");
            Decl* tagAssociatedTypeDecl = nullptr;
            if(auto enumTypeTypeDeclRefType = dynamicCast<DeclRefType>(enumTypeType))
            {
                if(auto enumTypeTypeInterfaceDecl = as<InterfaceDecl>(enumTypeTypeDeclRefType->declRef.getDecl()))
                {
                    for(auto memberDecl : enumTypeTypeInterfaceDecl->members)
                    {
                        if(memberDecl->getName() == tagAssociatedTypeName)
                        {
                            tagAssociatedTypeDecl = memberDecl;
                            break;
                        }
                    }
                }
            }
            if(!tagAssociatedTypeDecl)
            {
                SLANG_DIAGNOSE_UNEXPECTED(getSink(), decl, "failed to find built-in declaration '__Tag'");
            }

            // Okay, add the conformance witness for `__Tag` being satisfied by `tagType`
            witnessTable->requirementDictionary.Add(tagAssociatedTypeDecl, RequirementWitness(tagType));

            // TODO: we actually also need to synthesize a witness for the conformance of `tagType`
            // to the `__BuiltinIntegerType` interface, because that is a constraint on the
            // associated type `__Tag`.

            // TODO: eventually we should consider synthesizing other requirements for
            // the min/max tag values, or the total number of tags, so that people don't
            // have to declare these as additional cases.

            enumConformanceDecl->setCheckState(DeclCheckState::Checked);
        }
    }

    void SemanticsDeclBodyVisitor::visitEnumDecl(EnumDecl* decl)
    {
        auto enumType = DeclRefType::create(m_astBuilder, makeDeclRef(decl));

        auto tagType = decl->tagType;

        // Check the enum cases in order.
        for(auto caseDecl : decl->getMembersOfType<EnumCaseDecl>())
        {
            // Each case defines a value of the enum's type.
            //
            // TODO: If we ever support enum cases with payloads,
            // then they would probably have a type that is a
            // `FunctionType` from the payload types to the
            // enum type.
            //
            // TODO(tfoley): the case should grab its type  when
            // doing its own header checking, rather than rely on this...
            caseDecl->type.type = enumType;

            ensureDecl(caseDecl, DeclCheckState::Checked);
        }

        // For any enum case that didn't provide an explicit
        // tag value, derived an appropriate tag value.
        IntegerLiteralValue defaultTag = 0;
        for(auto caseDecl : decl->getMembersOfType<EnumCaseDecl>())
        {
            if(auto explicitTagValExpr = caseDecl->tagExpr)
            {
                // This tag has an initializer, so it should establish
                // the tag value for a successor case that doesn't
                // provide an explicit tag.

                IntVal* explicitTagVal = TryConstantFoldExpr(explicitTagValExpr);
                if(explicitTagVal)
                {
                    if(auto constIntVal = as<ConstantIntVal>(explicitTagVal))
                    {
                        defaultTag = constIntVal->value;
                    }
                    else
                    {
                        // TODO: need to handle other possibilities here
                        getSink()->diagnose(explicitTagValExpr, Diagnostics::unexpectedEnumTagExpr);
                    }
                }
                else
                {
                    // If this happens, then the explicit tag value expression
                    // doesn't seem to be a constant after all. In this case
                    // we expect the checking logic to have applied already.
                }
            }
            else
            {
                // This tag has no initializer, so it should use
                // the default tag value we are tracking.
                IntegerLiteralExpr* tagValExpr = m_astBuilder->create<IntegerLiteralExpr>();
                tagValExpr->loc = caseDecl->loc;
                tagValExpr->type = QualType(tagType);
                tagValExpr->value = defaultTag;

                caseDecl->tagExpr = tagValExpr;
            }

            // Default tag for the next case will be one more than
            // for the most recent case.
            //
            // TODO: We might consider adding a `[flags]` attribute
            // that modifies this behavior to be `defaultTagForCase <<= 1`.
            //
            defaultTag++;
        }
    }

    void SemanticsDeclBodyVisitor::visitEnumCaseDecl(EnumCaseDecl* decl)
    {
        // An enum case had better appear inside an enum!
        //
        // TODO: Do we need/want to support generic cases some day?
        auto parentEnumDecl = as<EnumDecl>(decl->parentDecl);
        SLANG_ASSERT(parentEnumDecl);

        // The tag type should have already been set by
        // the surrounding `enum` declaration.
        auto tagType = parentEnumDecl->tagType;
        SLANG_ASSERT(tagType);

        // Need to check the init expression, if present, since
        // that represents the explicit tag for this case.
        if(auto initExpr = decl->tagExpr)
        {
            initExpr = CheckExpr(initExpr);
            initExpr = coerce(tagType, initExpr);

            // We want to enforce that this is an integer constant
            // expression, but we don't actually care to retain
            // the value.
            CheckIntegerConstantExpression(initExpr);

            decl->tagExpr = initExpr;
        }
    }

    void SemanticsVisitor::ensureDeclBase(DeclBase* declBase, DeclCheckState state)
    {
        if(auto decl = as<Decl>(declBase))
        {
            ensureDecl(decl, state);
        }
        else if(auto declGroup = as<DeclGroup>(declBase))
        {
            for(auto dd : declGroup->decls)
            {
                ensureDecl(dd, state);
            }
        }
        else
        {
            SLANG_UNEXPECTED("unknown case for declaration");
        }
    }

    void SemanticsDeclHeaderVisitor::visitTypeDefDecl(TypeDefDecl* decl)
    {
        decl->type = CheckProperType(decl->type);
    }

    void SemanticsDeclHeaderVisitor::visitGlobalGenericParamDecl(GlobalGenericParamDecl* decl)
    {
        // global generic param only allowed in global scope
        auto program = as<ModuleDecl>(decl->parentDecl);
        if (!program)
            getSink()->diagnose(decl, Slang::Diagnostics::globalGenParamInGlobalScopeOnly);
    }

    void SemanticsDeclHeaderVisitor::visitAssocTypeDecl(AssocTypeDecl* decl)
    {
        // assoctype only allowed in an interface
        auto interfaceDecl = as<InterfaceDecl>(decl->parentDecl);
        if (!interfaceDecl)
            getSink()->diagnose(decl, Slang::Diagnostics::assocTypeInInterfaceOnly);
    }

    void SemanticsDeclBodyVisitor::visitFunctionDeclBase(FunctionDeclBase* decl)
    {
        if (auto body = decl->body)
        {
            checkBodyStmt(body, decl);
        }
    }

    void SemanticsVisitor::getGenericParams(
        GenericDecl*                        decl,
        List<Decl*>&                        outParams,
        List<GenericTypeConstraintDecl*>&   outConstraints)
    {
        for (auto dd : decl->members)
        {
            if (dd == decl->inner)
                continue;

            if (auto typeParamDecl = as<GenericTypeParamDecl>(dd))
                outParams.add(typeParamDecl);
            else if (auto valueParamDecl = as<GenericValueParamDecl>(dd))
                outParams.add(valueParamDecl);
            else if (auto constraintDecl = as<GenericTypeConstraintDecl>(dd))
                outConstraints.add(constraintDecl);
        }
    }

    bool SemanticsVisitor::doGenericSignaturesMatch(
        GenericDecl*                    left,
        GenericDecl*                    right,
        GenericSubstitution**    outSubstRightToLeft)
    {
        // Our first goal here is to determine if `left` and
        // `right` have equivalent lists of explicit
        // generic parameters.
        //
        // Once we have determined that the explicit generic
        // parameters match, we will look at the constraints
        // placed on those parameters to see if they are
        // equivalent.
        //
        // We thus start by extracting the explicit parameters
        // and the constraints from each declaration.
        //
        List<Decl*> leftParams;
        List<GenericTypeConstraintDecl*> leftConstraints;
        getGenericParams(left, leftParams, leftConstraints);

        List<Decl*> rightParams;
        List<GenericTypeConstraintDecl*> rightConstraints;
        getGenericParams(right, rightParams, rightConstraints);

        // For there to be any hope of a match, the two decls
        // need to have the same number of explicit parameters.
        //
        Index paramCount = leftParams.getCount();
        if(paramCount != rightParams.getCount())
            return false;

        // Next we will walk through the parameters and look
        // for a pair-wise match.
        //
        for(Index pp = 0; pp < paramCount; ++pp)
        {
            Decl* leftParam = leftParams[pp];
            Decl* rightParam = rightParams[pp];

            if (auto leftTypeParam = as<GenericTypeParamDecl>(leftParam))
            {
                if (auto rightTypeParam = as<GenericTypeParamDecl>(rightParam))
                {
                    // Right now any two type parameters are a match.
                    // Names are irrelevant to matching, and any constraints
                    // on the type parameters are represented as implicit
                    // extra parameters of the generic.
                    //
                    // TODO: If we ever supported type parameters with
                    // higher kinds we might need to make a check here
                    // that the kind of each parameter matches (which
                    // would in a sense be a kind of recursive check
                    // of the generic signature of the parameter).
                    //
                    continue;
                }
            }
            else if (auto leftValueParam = as<GenericValueParamDecl>(leftParam))
            {
                if (auto rightValueParam = as<GenericValueParamDecl>(rightParam))
                {
                    // In this case we have two generic value parameters,
                    // and they should only be considered to match if
                    // they have the same type.
                    //
                    // Note: We are assuming here that the type of a value
                    // parameter cannot be dependent on any of the type
                    // parameters in the same signature. This is a reasonable
                    // assumption for now, but could get thorny down the road.
                    //
                    if (!leftValueParam->getType()->equals(rightValueParam->getType()))
                    {
                        // If the value parameters have non-matching types,
                        // then the full generic signatures do not match.
                        //
                        return false;
                    }

                    // Generic value parameters with the same type are
                    // always considered to match.
                    //
                    continue;
                }
            }

            // If we get to this point, then we have two parameters that
            // were of different syntatic categories (e.g., one type parameter
            // and one value parameter), so the signatures clearly don't match.
            //
            return false;
        }

        // At this point we know that the explicit generic parameters
        // of `left` and `right` are aligned, but we need to check
        // that the constraints that each declaration places on
        // its parameters match.
        //
        // A first challenge that arises is that `left` and `right`
        // will each express the constraints in terms of their
        // own parameters. For example, consider the following
        // declarations:
        //
        //      void foo1<T : IFoo>(T value);
        //      void foo2<U : IFoo>(U value);
        //
        // It is "obvious" to a human that the signatures here
        // match, but `foo1` has a constraint `T : IFoo` while
        // `foo2` has a constraint `U : IFoo`, and since `T`
        // and `U` are distinct `Decl`s, those constraints
        // are not obviously equivalent.
        //
        // We will work around this first issue by creating
        // a substitution taht lists all the parameters of
        // `left`, which we can use to specialize `right`
        // so that it aligns.
        //
        // In terms of the example above, this is like constructing
        // `foo2<T>` so that its constraint, after specialization,
        // looks like `T : IFoo`.
        //
        auto& substRightToLeft = *outSubstRightToLeft;
        substRightToLeft = createDummySubstitutions(left);
        substRightToLeft->genericDecl = right;

        // We should now be able to enumerate the constraints
        // on `right` in a way that uses the same type parameters
        // as `left`, using `rightDeclRef`.
        //
        // At this point a second problem arises: if/when we support
        // more flexibility in how generic parameter constraints are
        // specified, it will be possible for two declarations to
        // list the "same" constraints in very different ways.
        //
        // For example, if we support a `where` clause for separating
        // the constraints from the parameters, then the following
        // two declarations should have equivalent signatures:
        //
        //      void foo1<T>(T value)
        //          where T : IFoo
        //      { ... }
        //
        //      void foo2<T : IFoo>(T value)
        //      { ... }
        //
        // Similarly, if we allow for general compositions of interfaces
        // to be used as constraints, then there can be more than one
        // way to specify the same constraints:
        //
        //      void foo1<T : IFoo&IBar>(T value);
        //      void foo2<T : IBar&IFoo>(T value);
        //
        // Adding support for equality constraints in `where` clauses
        // also creates opportunities for multiple equivalent expressions:
        //
        //      void foo1<T,U>(...) where T.A == U.A;
        //      void foo2<T,U>(...) where U.A == T.A;
        //
        // A robsut version of the checking logic here should attempt
        // to *canonicalize* all of the constraints. Canonicalization
        // should involve putting constraints into a deterministic
        // order (e.g., for a generic with `<T,U>` all the constraints
        // on `T` should come before those on `U`), rewriting individual
        // constraints into a canonical form (e.g., `T : IFoo & IBar`
        // should turn into two constraints: `T : IFoo` and `T : IBar`),
        // etc.
        //
        // Once the constraints are in a canonical form we should be able
        // to test them for pairwise equivalent. As a safety measure we
        // could also try to test whether one set of constraints implies
        // the other (since implication in both directions should imply
        // equivalence, in which case our canonicalization had better
        // have produced the same result).
        //
        // For now we are taking a simpler short-cut by assuming
        // that constraints are already in a canonical form, which
        // is reasonable for now as the syntax only allows a single
        // constraint per parameter, specified on the parameter itself.
        //
        // Under the assumption of canonical constraints, we can
        // assume that different numbers of constraints must indicate
        // a signature mismatch.
        //
        Index constraintCount = leftConstraints.getCount();
        if(constraintCount != rightConstraints.getCount())
            return false;

        for (Index cc = 0; cc < constraintCount; ++cc)
        {
            // Note that we use a plain `Decl` pointer for the left
            // constraint, but need to use a `DeclRef` for the right
            // constraint so that we can take the substitution
            // arguments into account.
            //
            GenericTypeConstraintDecl* leftConstraint = leftConstraints[cc];
            DeclRef<GenericTypeConstraintDecl> rightConstraint(rightConstraints[cc], substRightToLeft);

            // For now, every constraint has the form `sub : sup`
            // to indicate that `sub` must be a subtype of `sup`.
            //
            // Two such constraints are equivalent if their `sub`
            // and `sup` types are pairwise equivalent.
            //
            auto leftSub = leftConstraint->sub;
            auto rightSub = getSub(m_astBuilder, rightConstraint);
            if(!leftSub->equals(rightSub))
                return false;

            auto leftSup = leftConstraint->sup;
            auto rightSup = getSup(m_astBuilder, rightConstraint);
            if(!leftSup->equals(rightSup))
                return false;
        }

        // If we have checked all of the (canonicalized) constraints
        // and found them to be pairwise equivalent then the two
        // generic signatures seem to match.
        //
        return true;
    }

    bool SemanticsVisitor::doFunctionSignaturesMatch(
        DeclRef<FuncDecl> fst,
        DeclRef<FuncDecl> snd)
    {

        // TODO(tfoley): This copies the parameter array, which is bad for performance.
        auto fstParams = getParameters(fst).toArray();
        auto sndParams = getParameters(snd).toArray();

        // If the functions have different numbers of parameters, then
        // their signatures trivially don't match.
        auto fstParamCount = fstParams.getCount();
        auto sndParamCount = sndParams.getCount();
        if (fstParamCount != sndParamCount)
            return false;

        for (Index ii = 0; ii < fstParamCount; ++ii)
        {
            auto fstParam = fstParams[ii];
            auto sndParam = sndParams[ii];

            // If a given parameter type doesn't match, then signatures don't match
            if (!getType(m_astBuilder, fstParam)->equals(getType(m_astBuilder, sndParam)))
                return false;

            // If one parameter is `out` and the other isn't, then they don't match
            //
            // Note(tfoley): we don't consider `out` and `inout` as distinct here,
            // because there is no way for overload resolution to pick between them.
            if (fstParam.getDecl()->hasModifier<OutModifier>() != sndParam.getDecl()->hasModifier<OutModifier>())
                return false;

            // If one parameter is `ref` and the other isn't, then they don't match.
            //
            if(fstParam.getDecl()->hasModifier<RefModifier>() != sndParam.getDecl()->hasModifier<RefModifier>())
                return false;
        }

        // Note(tfoley): return type doesn't enter into it, because we can't take
        // calling context into account during overload resolution.

        return true;
    }

    GenericSubstitution* SemanticsVisitor::createDummySubstitutions(
        GenericDecl* genericDecl)
    {
        GenericSubstitution* subst = m_astBuilder->create<GenericSubstitution>();
        subst->genericDecl = genericDecl;
        for (auto dd : genericDecl->members)
        {
            if (dd == genericDecl->inner)
                continue;

            if (auto typeParam = as<GenericTypeParamDecl>(dd))
            {
                auto type = DeclRefType::create(m_astBuilder, makeDeclRef(typeParam));
                subst->args.add(type);
            }
            else if (auto valueParam = as<GenericValueParamDecl>(dd))
            {
                auto val = m_astBuilder->create<GenericParamIntVal>(
                    makeDeclRef(valueParam));
                subst->args.add(val);
            }
            // TODO: need to handle constraints here?
        }
        return subst;
    }

    // For simplicity we will make having a definition of a function include having a body or a target intrinsics defined.
    // It may be useful to add other modifiers to mark as having body - for example perhaps
    // any target intrinsic modifier (like SPIR-V version) should be included.
    //
    // Note that not having this check around TargetIntrinsicModifier can lead to a crash in the compiler
    // with a definition, followed by a declaration with a target intrinsic.
    // That this doesn't appear to be the case with other modifiers.
    // TODO: 
    // We may want to be able to add target intrinsics with other declarations, that being the case this logic
    // would need to change.
    // We might also want are more precise error that pointed out the actually problem - because strictly speaking
    // having a target intrinsic isn't a 'body'.
    bool _isDefinition(FuncDecl* decl)
    {
        return decl->body || decl->hasModifier<TargetIntrinsicModifier>();
    }

    Result SemanticsVisitor::checkFuncRedeclaration(
        FuncDecl* newDecl,
        FuncDecl* oldDecl)
    {
        // There are a few different cases that this function needs
        // to check for:
        //
        // * If `newDecl` and `oldDecl` have different signatures such
        //   that they can always be distinguished at call sites, then
        //   they don't conflict and don't count as redeclarations.
        //
        // * If `newDecl` and `oldDecl` have matching signatures, but
        //   differ in return type (or other details that would affect
        //   compatibility), then the declarations conflict and an
        //   error needs to be diagnosed.
        //
        // * If `newDecl` and `oldDecl` have matching/compatible sigantures,
        //   but differ when it comes to target-specific overloading,
        //   then they can co-exist.
        //
        // * If `newDecl` and `oldDecl` have matching/compatible signatures
        //   and are specialized for the same target(s), then only
        //   one can have a body (in which case the other is a forward declaration),
        //   or else we have a redefinition error.

        auto newGenericDecl = as<GenericDecl>(newDecl->parentDecl);
        auto oldGenericDecl = as<GenericDecl>(oldDecl->parentDecl);

        // If one declaration is a prefix/postfix operator, and the
        // other is not a matching operator, then don't consider these
        // to be re-declarations.
        //
        // Note(tfoley): Any attempt to call such an operator using
        // ordinary function-call syntax (if we decided to allow it)
        // would be ambiguous in such a case, of course.
        //
        if (newDecl->hasModifier<PrefixModifier>() != oldDecl->hasModifier<PrefixModifier>())
            return SLANG_OK;
        if (newDecl->hasModifier<PostfixModifier>() != oldDecl->hasModifier<PostfixModifier>())
            return SLANG_OK;

        // If one is generic and the other isn't, then there is no match.
        if ((newGenericDecl != nullptr) != (oldGenericDecl != nullptr))
            return SLANG_OK;

        // We are going to be comparing the signatures of the
        // two functions, but if they are *generic* functions
        // then we will need to compare them with consistent
        // specializations in place.
        //
        // We'll go ahead and create some (unspecialized) declaration
        // references here, just to be prepared.
        //
        DeclRef<FuncDecl> newDeclRef(newDecl, nullptr);
        DeclRef<FuncDecl> oldDeclRef(oldDecl, nullptr);

        // If we are working with generic functions, then we need to
        // consider if their generic signatures match.
        //
        if(newGenericDecl)
        {
            // If one declaration is generic, the other must be.
            // (This condition was already checked above)
            //
            SLANG_ASSERT(oldGenericDecl);

            // As part of checking if the generic signatures match,
            // we will produce a substitution that can be used to
            // reference `oldGenericDecl` with the generic parameters
            // substituted for those of `newDecl`.
            //
            // One way to think about it is that if we have these
            // declarations (ignore the name differences...):
            //
            //     // oldDecl:
            //     void foo1<T>(T x);
            //
            //     // newDecl:
            //     void foo2<U>(U x);
            //
            // Then we will compare the parameter types of `foo2`
            // against the specialization `foo1<U>`.
            //
            GenericSubstitution* subst = nullptr;
            if(!doGenericSignaturesMatch(newGenericDecl, oldGenericDecl, &subst))
                return SLANG_OK;

            oldDeclRef.substitutions.substitutions = subst;
        }

        // If the parameter signatures don't match, then don't worry
        if (!doFunctionSignaturesMatch(newDeclRef, oldDeclRef))
            return SLANG_OK;

        // If we get this far, then we've got two declarations in the same
        // scope, with the same name and signature, so they appear
        // to be redeclarations.
        //
        // We will track that redeclaration occured, so that we can
        // take it into account for overload resolution.
        //
        // A huge complication that we'll need to deal with is that
        // multiple declarations might introduce default values for
        // (different) parameters, and we might need to merge across
        // all of them (which could get complicated if defaults for
        // parameters can reference earlier parameters).

        // If the previous declaration wasn't already recorded
        // as being part of a redeclaration family, then make
        // it the primary declaration of a new family.
        if (!oldDecl->primaryDecl)
        {
            oldDecl->primaryDecl = oldDecl;
        }

        // The new declaration will belong to the family of
        // the previous one, and so it will share the same
        // primary declaration.
        newDecl->primaryDecl = oldDecl->primaryDecl;
        newDecl->nextDecl = nullptr;

        // Next we want to chain the new declaration onto
        // the linked list of redeclarations.
        auto link = &oldDecl->nextDecl;
        while (*link)
            link = &(*link)->nextDecl;
        *link = newDecl;

        // Now that we've added things to a group of redeclarations,
        // we can do some additional validation.

        // First, we will ensure that the return types match
        // between the declarations, so that they are truly
        // interchangeable.
        //
        // Note(tfoley): If we ever decide to add a beefier type
        // system to Slang, we might allow overloads like this,
        // so long as the desired result type can be disambiguated
        // based on context at the call type. In that case we would
        // consider result types earlier, as part of the signature
        // matching step.
        //
        auto resultType = getResultType(m_astBuilder, newDeclRef);
        auto prevResultType = getResultType(m_astBuilder, oldDeclRef);
        if (!resultType->equals(prevResultType))
        {
            // Bad redeclaration
            getSink()->diagnose(newDecl, Diagnostics::functionRedeclarationWithDifferentReturnType, newDecl->getName(), resultType, prevResultType);
            getSink()->diagnose(oldDecl, Diagnostics::seePreviousDeclarationOf, newDecl->getName());

            // Don't bother emitting other errors at this point
            return SLANG_FAIL;
        }

        // TODO: Enforce that the new declaration had better
        // not specify a default value for any parameter that
        // already had a default value in a prior declaration.

        // We are going to want to enforce that we cannot have
        // two declarations of a function both specify bodies.
        // Before we make that check, however, we need to deal
        // with the case where the two function declarations
        // might represent different target-specific versions
        // of a function.
        //
        // TODO: if the two declarations are specialized for
        // different targets, then skip the body checks below.
        //
        // ???: Why isn't this problem showing up in practice?

        // If both of the declarations have a body, then there
        // is trouble, because we wouldn't know which one to
        // use during code generation.
        if (_isDefinition(newDecl) && _isDefinition(oldDecl))
        {
            // Redefinition
            getSink()->diagnose(newDecl, Diagnostics::functionRedefinition, newDecl->getName());
            getSink()->diagnose(oldDecl, Diagnostics::seePreviousDefinitionOf, newDecl->getName());

            // Don't bother emitting other errors
            return SLANG_FAIL;
        }

        // At this point we've processed the redeclaration and
        // put it into a group, so there is no reason to keep
        // looping and looking at prior declarations.
        //
        // While no diagnostics have been emitted, we return
        // a failure result from the operation to indicate
        // to the caller that they should stop looping over
        // declarations at this point.
        //
        return SLANG_FAIL;
    }

    Result SemanticsVisitor::checkRedeclaration(Decl* newDecl, Decl* oldDecl)
    {
        // If either of the declarations being looked at is generic, then
        // we want to consider the "inner" declaration instead when
        // making decisions about what to allow or not.
        //
        if(auto newGenericDecl = as<GenericDecl>(newDecl))
            newDecl = newGenericDecl->inner;
        if(auto oldGenericDecl = as<GenericDecl>(oldDecl))
            oldDecl = oldGenericDecl->inner;

        // Functions are special in that we can have many declarations
        // with the same name in a given scope, and it is possible
        // for them to co-exist as overloads, or even just be multiple
        // declarations of the same function (thanks to the inherited
        // legacy of C forward declarations).
        //
        // If both declarations are functions, we will check that
        // they are allowed to co-exist using these more nuanced rules.
        //
        if( auto newFuncDecl = as<FuncDecl>(newDecl) )
        {
            if(auto oldFuncDecl = as<FuncDecl>(oldDecl) )
            {
                // Both new and old declarations are functions,
                // so redeclaration may be valid.
                return checkFuncRedeclaration(newFuncDecl, oldFuncDecl);
            }
        }

        // For all other flavors of declaration, we do not
        // allow duplicate declarations with the same name.
        //
        // TODO: We might consider allowing some other cases
        // of overloading that can be safely disambiguated:
        //
        // * A type and a value (function/variable/etc.) of the same name can usually
        //   co-exist because we can distinguish which is needed by context.
        //
        // * Multiple generic types with the same name can co-exist
        //   if their generic parameter lists are sufficient to
        //   tell them apart at a use site.

        // We will diagnose a redeclaration error at the new declaration,
        // and point to the old declaration for context.
        //
        getSink()->diagnose(newDecl, Diagnostics::redeclaration, newDecl->getName());
        getSink()->diagnose(oldDecl, Diagnostics::seePreviousDeclarationOf, oldDecl->getName());
        return SLANG_FAIL;
    }


    void SemanticsVisitor::checkForRedeclaration(Decl* decl)
    {
        // We want to consider a "new" declaration in the context
        // of some parent/container declaration, and compare it
        // to pre-existing "old" declarations of the same name
        // in the same container.
        //
        auto newDecl = decl;
        auto parentDecl = decl->parentDecl;

        // Sanity check: there should always be a parent declaration.
        //
        SLANG_ASSERT(parentDecl);
        if (!parentDecl) return;

        // If the declaration is the "inner" declaration of a generic,
        // then we actually want to look one level up, because the
        // peers/siblings of the declaration will belong to the same
        // parent as the generic, not to the generic.
        //
        if( auto genericParentDecl = as<GenericDecl>(parentDecl) )
        {
            // Note: we need to check here to be sure `newDecl`
            // is the "inner" declaration and not one of the
            // generic parameters, or else we will end up
            // checking them at the wrong scope.
            //
            if( newDecl == genericParentDecl->inner )
            {
                newDecl = parentDecl;
                parentDecl = genericParentDecl->parentDecl;
            }
        }

        // We will now look for other declarations with
        // the same name in the same parent/container.
        //
        buildMemberDictionary(parentDecl);
        for (auto oldDecl = newDecl->nextInContainerWithSameName; oldDecl; oldDecl = oldDecl->nextInContainerWithSameName)
        {
            // For each matching declaration, we will check
            // whether the redeclaration should be allowed,
            // and emit an appropriate diagnostic if not.
            //
            Result checkResult = checkRedeclaration(newDecl, oldDecl);

            // The `checkRedeclaration` function will return a failure
            // status (whether or not it actually emitted a diagnostic)
            // if we should stop checking further redeclarations, because
            // the declaration in question has been dealt with fully.
            //
            if(SLANG_FAILED(checkResult))
                break;
        }
    }


    void SemanticsDeclHeaderVisitor::visitParamDecl(ParamDecl* paramDecl)
    {
        // TODO: This logic should be shared with the other cases of
        // variable declarations. The main reason I am not doing it
        // yet is that we use a `ParamDecl` with a null type as a
        // special case in attribute declarations, and that could
        // trip up the ordinary variable checks.

        auto typeExpr = paramDecl->type;
        if(typeExpr.exp)
        {
            typeExpr = CheckUsableType(typeExpr);
            paramDecl->type = typeExpr;
        }
    }

    void SemanticsDeclBodyVisitor::visitParamDecl(ParamDecl* paramDecl)
    {
        auto typeExpr = paramDecl->type;

        // The "initializer" expression for a parameter represents
        // a default argument value to use if an explicit one is
        // not supplied.
        if(auto initExpr = paramDecl->initExpr)
        {
            // We must check the expression and coerce it to the
            // actual type of the parameter.
            //
            initExpr = CheckExpr(initExpr);
            initExpr = coerce(typeExpr.type, initExpr);
            paramDecl->initExpr = initExpr;

            // TODO: a default argument expression needs to
            // conform to other constraints to be valid.
            // For example, it should not be allowed to refer
            // to other parameters of the same function (or maybe
            // only the parameters to its left...).

            // A default argument value should not be allowed on an
            // `out` or `inout` parameter.
            //
            // TODO: we could relax this by requiring the expression
            // to yield an lvalue, but that seems like a feature
            // with limited practical utility (and an easy source
            // of confusing behavior).
            //
            // Note: the `InOutModifier` class inherits from `OutModifier`,
            // so we only need to check for the base case.
            //
            if(paramDecl->findModifier<OutModifier>())
            {
                getSink()->diagnose(initExpr, Diagnostics::outputParameterCannotHaveDefaultValue);
            }
        }
    }

    void SemanticsDeclHeaderVisitor::checkCallableDeclCommon(CallableDecl* decl)
    {
        for(auto& paramDecl : decl->getParameters())
        {
            ensureDecl(paramDecl, DeclCheckState::ReadyForReference);
        }
    }

    void SemanticsDeclHeaderVisitor::visitFuncDecl(FuncDecl* funcDecl)
    {
        auto resultType = funcDecl->returnType;
        if(resultType.exp)
        {
            resultType = CheckProperType(resultType);
        }
        else
        {
            resultType = TypeExp(m_astBuilder->getVoidType());
        }
        funcDecl->returnType = resultType;

        checkCallableDeclCommon(funcDecl);
    }

    IntegerLiteralValue SemanticsVisitor::GetMinBound(IntVal* val)
    {
        if (auto constantVal = as<ConstantIntVal>(val))
            return constantVal->value;

        // TODO(tfoley): Need to track intervals so that this isn't just a lie...
        return 1;
    }

    void SemanticsVisitor::maybeInferArraySizeForVariable(VarDeclBase* varDecl)
    {
        // Not an array?
        auto arrayType = as<ArrayExpressionType>(varDecl->type);
        if (!arrayType) return;

        // Explicit element count given?
        auto elementCount = arrayType->arrayLength;
        if (elementCount) return;

        // No initializer?
        auto initExpr = varDecl->initExpr;
        if(!initExpr) return;

        // Is the type of the initializer an array type?
        if(auto arrayInitType = as<ArrayExpressionType>(initExpr->type))
        {
            elementCount = arrayInitType->arrayLength;
        }
        else
        {
            // Nothing to do: we couldn't infer a size
            return;
        }

        // Create a new array type based on the size we found,
        // and install it into our type.
        varDecl->type.type = getArrayType(
            m_astBuilder,
            arrayType->baseType,
            elementCount);
    }

    void SemanticsVisitor::validateArraySizeForVariable(VarDeclBase* varDecl)
    {
        auto arrayType = as<ArrayExpressionType>(varDecl->type);
        if (!arrayType) return;

        auto elementCount = arrayType->arrayLength;
        if (!elementCount)
        {
            // Note(tfoley): For now we allow arrays of unspecified size
            // everywhere, because some source languages (e.g., GLSL)
            // allow them in specific cases.
#if 0
            getSink()->diagnose(varDecl, Diagnostics::invalidArraySize);
#endif
            return;
        }

        // TODO(tfoley): How to handle the case where bound isn't known?
        if (GetMinBound(elementCount) <= 0)
        {
            getSink()->diagnose(varDecl, Diagnostics::invalidArraySize);
            return;
        }
    }

    void SemanticsDeclBasesVisitor::visitExtensionDecl(ExtensionDecl* decl)
    {
        decl->targetType = CheckProperType(decl->targetType);

        if (auto targetDeclRefType = as<DeclRefType>(decl->targetType))
        {
            // Attach our extension to that type as a candidate...
            if (auto aggTypeDeclRef = targetDeclRefType->declRef.as<AggTypeDecl>())
            {
                auto aggTypeDecl = aggTypeDeclRef.getDecl();
                decl->nextCandidateExtension = aggTypeDecl->candidateExtensions;
                aggTypeDecl->candidateExtensions = decl;
                return;
            }
        }
        getSink()->diagnose(decl->targetType.exp, Diagnostics::unimplemented, "expected a nominal type here");
    }

    Type* SemanticsVisitor::calcThisType(DeclRef<Decl> declRef)
    {
        if( auto interfaceDeclRef = declRef.as<InterfaceDecl>() )
        {
            // In the body of an `interface`, a `This` type
            // refers to the concrete type that will eventually
            // conform to the interface and fill in its
            // requirements.
            //
            ThisType* thisType = m_astBuilder->create<ThisType>();
            thisType->interfaceDeclRef = interfaceDeclRef;
            return thisType;
        }
        else if (auto aggTypeDeclRef = declRef.as<AggTypeDecl>())
        {
            // In the body of an ordinary aggregate type,
            // such as a `struct`, the `This` type just
            // refers to the type itself.
            //
            // TODO: If/when we support `class` types
            // with inheritance, then `This` inside a class
            // would need to refer to the eventual concrete
            // type, much like the `interface` case above.
            //
            return DeclRefType::create(m_astBuilder, aggTypeDeclRef);
        }
        else if (auto extDeclRef = declRef.as<ExtensionDecl>())
        {
            // In the body of an `extension`, the `This`
            // type refers to the type being extended.
            //
            // Note: we currently have this loop back
            // around through `calcThisType` for the
            // type being extended, rather than just
            // using it directly. This makes a difference
            // for polymorphic types like `interface`s,
            // and there are reasonable arguments for
            // the validity of either option.
            //
            // Does `extension IFoo` mean extending
            // exactly the type `IFoo` (an existential,
            // which could at runtime be a value of
            // any type conforming to `IFoo`), or does
            // it implicitly extend every type that
            // conforms to `IFoo`? The difference is
            // significant, and we need to make a choice
            // sooner or later.
            //
            ensureDecl(extDeclRef, DeclCheckState::CanUseExtensionTargetType);
            auto targetType = getTargetType(m_astBuilder, extDeclRef);
            return calcThisType(targetType);
        }
        else
        {
            return nullptr;
        }
    }

    Type* SemanticsVisitor::calcThisType(Type* type)
    {
        if( auto declRefType = as<DeclRefType>(type) )
        {
            return calcThisType(declRefType->declRef);
        }
        else
        {
            return type;
        }
    }

    Type* SemanticsVisitor::findResultTypeForConstructorDecl(ConstructorDecl* decl)
    {
        // We want to look at the parent of the declaration,
        // but if the declaration is generic, the parent will be
        // the `GenericDecl` and we need to skip past that to
        // the grandparent.
        //
        auto parent = decl->parentDecl;
        auto genericParent = as<GenericDecl>(parent);
        if (genericParent)
        {
            parent = genericParent->parentDecl;
        }

        // The result type for a constructor is whatever `This` would
        // refer to in the body of the outer declaration.
        //
        auto thisType = calcThisType(makeDeclRef(parent));
        if( !thisType )
        {
            getSink()->diagnose(decl, Diagnostics::initializerNotInsideType);
            thisType = m_astBuilder->getErrorType();
        }
        return thisType;
    }

    void SemanticsDeclHeaderVisitor::visitConstructorDecl(ConstructorDecl* decl)
    {
        // We need to compute the result tyep for this declaration,
        // since it wasn't filled in for us.
        decl->returnType.type = findResultTypeForConstructorDecl(decl);

        checkCallableDeclCommon(decl);
    }

    void SemanticsDeclHeaderVisitor::visitSubscriptDecl(SubscriptDecl* decl)
    {
        decl->returnType = CheckUsableType(decl->returnType);

        // If we have a subscript declaration with no accessor declarations,
        // then we should create a single `GetterDecl` to represent
        // the implicit meaning of their declaration, so:
        //
        //      subscript(uint index) -> T;
        //
        // becomes:
        //
        //      subscript(uint index) -> T { get; }
        //

        bool anyAccessors = decl->getMembersOfType<AccessorDecl>().isNonEmpty();

        if(!anyAccessors)
        {
            GetterDecl* getterDecl = m_astBuilder->create<GetterDecl>();
            getterDecl->loc = decl->loc;

            getterDecl->parentDecl = decl;
            decl->members.add(getterDecl);
        }

        checkCallableDeclCommon(decl);
    }

    void SemanticsDeclHeaderVisitor::visitAccessorDecl(AccessorDecl* decl)
    {
        // An accessor must appear nested inside a subscript declaration (today),
        // or a property declaration (when we add them). It will derive
        // its return type from the outer declaration, so we handle both
        // of these checks at the same place.
        auto parent = decl->parentDecl;
        if (auto parentSubscript = as<SubscriptDecl>(parent))
        {
            ensureDecl(parentSubscript, DeclCheckState::CanUseTypeOfValueDecl);
            decl->returnType = parentSubscript->returnType;
        }
        // TODO: when we add "property" declarations, check for them here
        else
        {
            getSink()->diagnose(decl, Diagnostics::accessorMustBeInsideSubscriptOrProperty);
        }

        checkCallableDeclCommon(decl);
    }

    GenericDecl* SemanticsVisitor::GetOuterGeneric(Decl* decl)
    {
        auto parentDecl = decl->parentDecl;
        if (!parentDecl) return nullptr;
        auto parentGeneric = as<GenericDecl>(parentDecl);
        return parentGeneric;
    }

    DeclRef<ExtensionDecl> SemanticsVisitor::ApplyExtensionToType(
        ExtensionDecl*  extDecl,
        Type*    type)
    {
        DeclRef<ExtensionDecl> extDeclRef = makeDeclRef(extDecl);

        // If the extension is a generic extension, then we
        // need to infer type arguments that will give
        // us a target type that matches `type`.
        //
        if (auto extGenericDecl = GetOuterGeneric(extDecl))
        {
            ConstraintSystem constraints;
            constraints.loc = extDecl->loc;
            constraints.genericDecl = extGenericDecl;

            if (!TryUnifyTypes(constraints, extDecl->targetType.Ptr(), type))
                return DeclRef<ExtensionDecl>();

            auto constraintSubst = TrySolveConstraintSystem(&constraints, DeclRef<Decl>(extGenericDecl, nullptr).as<GenericDecl>());
            if (!constraintSubst)
            {
                return DeclRef<ExtensionDecl>();
            }

            // Construct a reference to the extension with our constraint variables
            // set as they were found by solving the constraint system.
            extDeclRef = DeclRef<Decl>(extDecl, constraintSubst).as<ExtensionDecl>();
        }

        // Now extract the target type from our (possibly specialized) extension decl-ref.
        Type* targetType = getTargetType(m_astBuilder, extDeclRef);

        // As a bit of a kludge here, if the target type of the extension is
        // an interface, and the `type` we are trying to match up has a this-type
        // substitution for that interface, then we want to attach a matching
        // substitution to the extension decl-ref.
        if(auto targetDeclRefType = as<DeclRefType>(targetType))
        {
            if(auto targetInterfaceDeclRef = targetDeclRefType->declRef.as<InterfaceDecl>())
            {
                // Okay, the target type is an interface.
                //
                // Is the type we want to apply to also an interface?
                if(auto appDeclRefType = as<DeclRefType>(type))
                {
                    if(auto appInterfaceDeclRef = appDeclRefType->declRef.as<InterfaceDecl>())
                    {
                        if(appInterfaceDeclRef.getDecl() == targetInterfaceDeclRef.getDecl())
                        {
                            // Looks like we have a match in the types,
                            // now let's see if we have a this-type substitution.
                            if(auto appThisTypeSubst = as<ThisTypeSubstitution>(appInterfaceDeclRef.substitutions.substitutions))
                            {
                                if(appThisTypeSubst->interfaceDecl == appInterfaceDeclRef.getDecl())
                                {
                                    // The type we want to apply to has a this-type substitution,
                                    // and (by construction) the target type currently does not.
                                    //
                                    SLANG_ASSERT(!as<ThisTypeSubstitution>(targetInterfaceDeclRef.substitutions.substitutions));

                                    // We will create a new substitution to apply to the target type.
                                    ThisTypeSubstitution* newTargetSubst = m_astBuilder->create<ThisTypeSubstitution>();
                                    newTargetSubst->interfaceDecl = appThisTypeSubst->interfaceDecl;
                                    newTargetSubst->witness = appThisTypeSubst->witness;
                                    newTargetSubst->outer = targetInterfaceDeclRef.substitutions.substitutions;

                                    targetType = DeclRefType::create(m_astBuilder,
                                        DeclRef<InterfaceDecl>(targetInterfaceDeclRef.getDecl(), newTargetSubst));

                                    // Note: we are constructing a this-type substitution that
                                    // we will apply to the extension declaration as well.
                                    // This is not strictly allowed by our current representation
                                    // choices, but we need it in order to make sure that
                                    // references to the target type of the extension
                                    // declaration have a chance to resolve the way we want them to.

                                    ThisTypeSubstitution* newExtSubst = m_astBuilder->create<ThisTypeSubstitution>();
                                    newExtSubst->interfaceDecl = appThisTypeSubst->interfaceDecl;
                                    newExtSubst->witness = appThisTypeSubst->witness;
                                    newExtSubst->outer = extDeclRef.substitutions.substitutions;

                                    extDeclRef = DeclRef<ExtensionDecl>(
                                        extDeclRef.getDecl(),
                                        newExtSubst);

                                    // TODO: Ideally we should also apply the chosen specialization to
                                    // the decl-ref for the extension, so that subsequent lookup through
                                    // the members of this extension will retain that substitution and
                                    // be able to apply it.
                                    //
                                    // E.g., if an extension method returns a value of an associated
                                    // type, then we'd want that to become specialized to a concrete
                                    // type when using the extension method on a value of concrete type.
                                    //
                                    // The challenge here that makes me reluctant to just staple on
                                    // such a substitution is that it wouldn't follow our implicit
                                    // rules about where `ThisTypeSubstitution`s can appear.
                                }
                            }
                        }
                    }
                }
            }
        }

        // In order for this extension to apply to the given type, we
        // need to have a match on the target types.
        if (!type->equals(targetType))
            return DeclRef<ExtensionDecl>();


        return extDeclRef;
    }

    QualType SemanticsVisitor::GetTypeForDeclRef(DeclRef<Decl> declRef, SourceLoc loc)
    {
        Type* typeResult = nullptr;
        return getTypeForDeclRef(
            m_astBuilder,
            this,
            getSink(),
            declRef,
            &typeResult,
            loc);
    }

    void SemanticsVisitor::importModuleIntoScope(Scope* scope, ModuleDecl* moduleDecl)
    {
        // If we've imported this one already, then
        // skip the step where we modify the current scope.
        auto& importedModules = getShared()->importedModules;
        if (importedModules.Contains(moduleDecl))
        {
            return;
        }
        importedModules.Add(moduleDecl);


        // Create a new sub-scope to wire the module
        // into our lookup chain.
        auto subScope = new Scope();
        subScope->containerDecl = moduleDecl;

        subScope->nextSibling = scope->nextSibling;
        scope->nextSibling = subScope;

        // Also import any modules from nested `import` declarations
        // with the `__exported` modifier
        for (auto importDecl : moduleDecl->getMembersOfType<ImportDecl>())
        {
            if (!importDecl->hasModifier<ExportedModifier>())
                continue;

            importModuleIntoScope(scope, importDecl->importedModuleDecl);
        }
    }

    void SemanticsDeclHeaderVisitor::visitImportDecl(ImportDecl* decl)
    {
        // We need to look for a module with the specified name
        // (whether it has already been loaded, or needs to
        // be loaded), and then put its declarations into
        // the current scope.

        auto name = decl->moduleNameAndLoc.name;
        auto scope = decl->scope;

        // Try to load a module matching the name
        auto importedModule = findOrImportModule(
            getLinkage(),
            name,
            decl->moduleNameAndLoc.loc,
            getSink());

        // If we didn't find a matching module, then bail out
        if (!importedModule)
            return;

        // Record the module that was imported, so that we can use
        // it later during code generation.
        auto importedModuleDecl = importedModule->getModuleDecl();
        decl->importedModuleDecl = importedModuleDecl;

        // Add the declarations from the imported module into the scope
        // that the `import` declaration is set to extend.
        //
        importModuleIntoScope(scope.Ptr(), importedModuleDecl);

        // Record the `import`ed module (and everything it depends on)
        // as a dependency of the module we are compiling.
        if(auto module = getModule(decl))
        {
            module->addModuleDependency(importedModule);
        }
    }

    static void _dispatchDeclCheckingVisitor(Decl* decl, DeclCheckState state, SharedSemanticsContext* shared)
    {
        switch(state)
        {
        case DeclCheckState::ModifiersChecked:
            SemanticsDeclModifiersVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::SignatureChecked:
            SemanticsDeclHeaderVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::ReadyForReference:
            SemanticsDeclRedeclarationVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::ReadyForLookup:
            SemanticsDeclBasesVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::ReadyForConformances:
            SemanticsDeclConformancesVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::Checked:
            SemanticsDeclBodyVisitor(shared).dispatch(decl);
            break;
        }
    }

}
