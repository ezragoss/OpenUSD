//
// Copyright 2023 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#ifndef PXR_USD_SDF_PATH_EXPRESSION_H
#define PXR_USD_SDF_PATH_EXPRESSION_H

#include "pxr/pxr.h"
#include "pxr/usd/sdf/api.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/predicateExpression.h"

#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// \class SdfPathExpression
///
/// Objects of this class represent a logical expression syntax tree consisting
/// of SdfPath matching patterns (with optionally embedded predicate
/// expressions) joined by the set-algebraic operators '+' (union), '&'
/// (intersection), '-' (difference), '~' (complement) and an implied-union
/// operator represented by two subexpressions joined by whitespace.
///
/// An SdfPathExpression can be constructed from a string, which will parse the
/// string into an expression object.  The syntax for an expression is as
/// follows:
///
/// The fundamental building blocks are path patterns and expression references.
/// A path pattern is similar to an SdfPath, but it may contain glob-style
/// wild-card characters, embedded brace-enclosed predicate expressions (see
/// SdfPredicateExpression) and '//' elements indicating arbitrary levels of
/// prim hierarchy.  For example, consider "/foo//bar*/baz{active:false}".  This
/// pattern matches absolute paths whose first component is 'foo', that also
/// have some descendant prim whose name begins with 'bar', which in turn has a
/// child named 'baz' where the predicate "active:false" evaluates to true.
///
/// An expression reference starts with '%' followed by a prim path, a ':', and
/// a name.  There is also one "special" expression reference, "%_" which means
/// "the weaker" expression when composing expressions together.  See
/// ComposeOver() and ResolveReferences() for more information.
///
/// These building blocks may be joined as mentioned above, with '+', '-', '&',
/// or whitespace, and may be complemented with '~', and grouped with '(' and
/// ')'.
class SdfPathExpression
{
public:
    /// \class PathPattern
    ///
    /// Objects of this class represent SdfPath matching patterns, consisting of
    /// an SdfPath prefix followed by a sequence of components, which may
    /// contain wildcards and optional embedded predicate expressions (see
    /// SdfPredicateExpression).
    class PathPattern
    {
    public:
        /// Construct the empty pattern whose bool-conversion operator returns
        /// false.
        SDF_API
        PathPattern();

        /// A component represents a pattern matching component past the initial
        /// SdfPath prefix.  A component's text can contain wildcard characters,
        /// and if the component references a predicate expression, its
        /// predicateIndex indicates which one in the owning PathPattern's list
        /// of expressions.  A component whose text is empty represents an
        /// "arbitrary levels of hierarchy" element (the //) in a path pattern.
        struct Component {
            bool IsStretch() const {
                return predicateIndex == -1 && text.empty();
            }
            std::string text;
            int predicateIndex = -1;
            bool isLiteral = false;
        };

        /// Append a prim child component to this pattern, with optional
        /// predicate expression \p predExpr.  If this pattern does not yet
        /// contain any wildcards or components with predicate expressions, and
        /// the input text does not contain wildcards, and \p predExpr is empty,
        /// then append a child component to this pattern's prefix path (see
        /// GetPrefix()).  Otherwise append this component to the sequence of
        /// components.
        SDF_API
        void AppendChild(std::string const &text,
                         SdfPredicateExpression &&predExpr);
        /// \overload
        SDF_API
        void AppendChild(std::string const &text,
                         SdfPredicateExpression const &predExpr);
        /// \overload
        SDF_API
        void AppendChild(std::string const &text);

        /// Append a prim property component to this pattern, with optional
        /// predicate expression \p predExpr.  If this pattern does not yet
        /// contain any wildcards or components with predicate expressions, and
        /// the input text does not contain wildcards, and \p predExpr is empty,
        /// then append a property component to this pattern's prefix path (see
        /// GetPrefix()). Otherwise append this component to the sequence of
        /// components.
        SDF_API
        void AppendProperty(std::string const &text,
                            SdfPredicateExpression &&predExpr);
        /// \overload
        SDF_API
        void AppendProperty(std::string const &text,
                            SdfPredicateExpression const &predExpr);
        /// \overload
        SDF_API
        void AppendProperty(std::string const &text);

        /// Return this pattern's non-speculative prefix (leading path
        /// components with no wildcards and no predicates).
        SdfPath const &GetPrefix() const & {
            return _prefix;
        }

        /// \overload.
        SdfPath GetPrefix() && {
            return std::move(_prefix);
        }

        /// Set this pattern's non-speculative prefix (leading path
        /// components with no wildcards and no predicates).
        SDF_API
        void SetPrefix(SdfPath &&p);

        /// \overload
        void SetPrefix(SdfPath const &p) {
            SetPrefix(SdfPath(p));
        }

        /// Return a debugging-oriented string representation of this pattern.
        SDF_API
        std::string GetDebugString() const;

        std::vector<Component> const &GetComponents() const & {
            return _components;
        }
        
        std::vector<Component> GetComponents() && {
            return _components;
        }

        std::vector<SdfPredicateExpression> const &
        GetPredicateExprs() const & {
            return _predExprs;
        }
        
        std::vector<SdfPredicateExpression>
        GetPredicateExprs() && {
            return _predExprs;
        }

        bool IsProperty() const {
            return _isProperty;
        }

        /// Return true if this pattern is not empty, false if it is.
        explicit operator bool() const {
            return !_prefix.IsEmpty();
        }

    private:
        SdfPath _prefix;
        std::vector<Component> _components;
        std::vector<SdfPredicateExpression> _predExprs;
        bool _isProperty;
    };

    /// \class ExpressionReference
    ///
    /// Objects of this class represent references to other path expressions,
    /// which will be resolved later by a call to ResolveReferences() or
    /// ComposeOver().
    class ExpressionReference {
    public:
        // Optional path reference, can be empty for "weaker" references (name
        // is "_") or for references to local collections.
        SdfPath path;
        
        // Name is either a property name, or "_" (meaning the weaker
        // collection).  If the name is "_", the path must be empty.
        std::string name;
    };

    /// Enumerant describing a subexpression operation.
    enum Op {
        // Operations on atoms.
        Complement,
        ImpliedUnion,
        Union,
        Intersection,
        Difference,

        // Atoms.
        ExpressionRef,
        Pattern
    };

    /// Default construction produces the "empty" expression.  Conversion to
    /// bool returns 'false'.
    SdfPathExpression() = default;

    /// Construct an expression by parsing \p expr.  If provided, \p
    /// parseContext appears in a parse error, if one is generated.  See
    /// GetParseError().  See the class documentation for details on expression
    /// syntax.
    SDF_API
    explicit SdfPathExpression(std::string const &expr,
                               std::string const &parseContext = {});

    /// Produce a new expression representing the set-complement of \p right.
    SDF_API
    static SdfPathExpression
    MakeComplement(SdfPathExpression &&right);

    /// \overload
    static SdfPathExpression
    MakeComplement(SdfPathExpression const &right) {
        return MakeComplement(SdfPathExpression(right));
    }

    /// Produce a new expression representing the set-algebraic operation \p op
    /// with operands \p left and \p right.  The \p op must be one of
    /// ImpliedUnion, Union, Intersection, or Difference.
    SDF_API
    static SdfPathExpression
    MakeOp(Op op, SdfPathExpression &&left, SdfPathExpression &&right);

    /// \overload
    static SdfPathExpression
    MakeOp(Op op,
           SdfPathExpression const &left,
           SdfPathExpression const &right) {
        return MakeOp(op, SdfPathExpression(left), SdfPathExpression(right));
    }

    /// Produce a new expression containing only the reference \p ref.
    SDF_API
    static SdfPathExpression
    MakeAtom(ExpressionReference &&ref);

    /// \overload
    static SdfPathExpression
    MakeAtom(ExpressionReference const &ref) {
        return MakeAtom(ExpressionReference(ref));
    }

    /// Produce a new expression containing only the pattern \p pattern.
    SDF_API
    static SdfPathExpression
    MakeAtom(PathPattern &&pattern);

    /// \overload
    static SdfPathExpression
    MakeAtom(PathPattern const &pattern) {
        return MakeAtom(PathPattern(pattern));
    }

    /// Walk this expression's syntax tree in depth-first order, calling \p
    /// pattern with the current PathPattern when one is encountered, \p ref
    /// with the current ExpressionReference when one is encountered, and \p
    /// logic multiple times for each logical operation encountered.  When
    /// calling \p logic, the logical operation is passed as the \p Op
    /// parameter, and an integer indicating "where" we are in the set of
    /// operands is passed as the int parameter. For a Complement, call \p
    /// logic(Op=Complement, int=0) to start, then after the subexpression that
    /// the Complement applies to is walked, call \p logic(Op=Complement,
    /// int=1).  For the other operators like Union and Intersection, call \p
    /// logic(Op, 0) before the first argument, then \p logic(Op, 1) after the
    /// first subexpression, then \p logic(Op, 2) after the second
    /// subexpression.  For a concrete example, consider the following
    /// expression:
    ///
    /// /foo/bar// /foo/baz// & ~/foo/bar/qux// %_
    ///
    /// logic(Intersection, 0)
    /// logic(ImpliedUnion, 0)
    /// pattern(/foo/bar//)
    /// logic(ImpliedUnion, 1)
    /// pattern(/foo/baz//)
    /// logic(ImpliedUnion, 2)
    /// logic(Intersection, 1)
    /// logic(ImpliedUnion, 0)
    /// logic(Complement, 0)
    /// pattern(/foo/bar/qux//)
    /// logic(Complement, 1)
    /// logic(ImpliedUnion, 1)
    /// ref(%_)
    /// logic(ImpliedUnion, 2)
    /// logic(Intersection, 2)
    /// 
    SDF_API
    void Walk(TfFunctionRef<void (Op, int)> logic,
              TfFunctionRef<void (ExpressionReference const &)> ref,
              TfFunctionRef<void (PathPattern const &)> pattern) const;

    /// Return a new expression created by replacing literal path prefixes that
    /// start with \p oldPrefix with \p newPrefix.
    SdfPathExpression
    ReplacePrefix(SdfPath const &oldPrefix,
                  SdfPath const &newPrefix) const & {
        return SdfPathExpression(*this).ReplacePrefix(oldPrefix, newPrefix);
    }

    /// Return a new expression created by replacing literal path prefixes that
    /// start with \p oldPrefix with \p newPrefix.
    SDF_API
    SdfPathExpression
    ReplacePrefix(SdfPath const &oldPrefix,
                  SdfPath const &newPrefix) &&;

    /// Return true if all contained pattern prefixes are absolute, false
    /// otherwise.  Call MakeAbsolute() to anchor any relative paths and make
    /// them absolute.
    SDF_API
    bool IsAbsolute() const;

    /// Return a new expression created by making any relative path prefixes in
    /// this expression absolute by SdfPath::MakeAbsolutePath().
    SdfPathExpression
    MakeAbsolute(SdfPath const &anchor) const & {
        return SdfPathExpression(*this).MakeAbsolute(anchor);
    }

    /// Return a new expression created by making any relative path prefixes in
    /// this expression absolute by SdfPath::MakeAbsolutePath().
    SDF_API
    SdfPathExpression
    MakeAbsolute(SdfPath const &anchor) &&;

    /// Return true if this expression contains any references to other
    /// collections.
    bool ContainsExpressionReferences() const {
        return !_refs.empty();
    }

    /// Return a new expression created by resolving collection references in
    /// this expression. Call \p resolve to produce an subexpression from a
    /// %-referenced collection.  By convention, the empty SdfPath represents
    /// the "weaker" %_ reference.  If \p resolve returns the empty
    /// SdfPathExpression, the reference is not resolved and persists in the
    /// output.
    SdfPathExpression
    ResolveReferences(
        TfFunctionRef<SdfPathExpression (
                          ExpressionReference const &)> resolve) const & {
        return SdfPathExpression(*this).ResolveReferences(resolve);
    }

    /// \overload
    SDF_API
    SdfPathExpression
    ResolveReferences(
        TfFunctionRef<SdfPathExpression (
                          ExpressionReference const &)> resolve) &&;
    
    /// Return a new expression created by replacing references to the "weaker
    /// expression" (i.e. %_) in this expression with \p weaker.  This is a
    /// restricted form of ResolveReferences() that only resolves "weaker"
    /// references, replacing them by \p weaker, leaving other references
    /// unmodified.
    SdfPathExpression
    ComposeOver(SdfPathExpression const &weaker) const & {
        return SdfPathExpression(*this).ComposeOver(weaker);
    }

    /// \overload
    SDF_API
    SdfPathExpression
    ComposeOver(SdfPathExpression const &weaker) &&;

    /// Return true if this expression is considered "complete".  Here, complete
    /// means that the expression has all absolute paths, and contains no
    /// expression references.  This is equivalent to:
    ///
    /// \code
    /// !expr.ContainsExpressionReferences() && expr.IsAbsolute()
    /// \endcode
    ///
    /// To complete an expression, call MakeAbsolute(), ResolveReferences()
    /// and/or ComposeOver().
    bool IsComplete() const {
        return !ContainsExpressionReferences() && IsAbsolute();
    }

    /// Return a debugging-oriented string representation of this expression.
    SDF_API
    std::string GetDebugString() const;

    /// Return true if this is the empty expression; i.e. default-constructed or
    /// constructed from a string with invalid syntax.
    bool IsEmpty() const {
        return _ops.empty();
    }
    
    /// Return true if this expression contains any operations, false otherwise.
    explicit operator bool() const {
        return !IsEmpty();
    }

    /// Return parsing errors as a string if this function was constructed from
    /// a string and parse errors were encountered.
    std::string const &GetParseError() const & {
        return _parseError;
    }

private:

    std::vector<Op> _ops;
    std::vector<ExpressionReference> _refs;
    std::vector<PathPattern> _patterns;

    // This member holds a parsing error string if this expression was
    // constructed by the parser and errors were encountered during the parsing.
    std::string _parseError;    
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_SDF_PATH_EXPRESSION_H
