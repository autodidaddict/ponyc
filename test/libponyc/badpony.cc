#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


/** Pony code that parses, but is erroneous. Typically type check errors and
 * things used in invalid contexts.
 *
 * We build all the way up to and including code gen and check that we do not
 * assert, segfault, etc but that the build fails and at least one error is
 * reported.
 *
 * There is definite potential for overlap with other tests but this is also a
 * suitable location for tests which don't obviously belong anywhere else.
 */

#define TEST_COMPILE(src) DO(test_compile(src, "all"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }


class BadPonyTest : public PassTest
{};


// Cases from reported issues

TEST_F(BadPonyTest, ClassInOtherClassProvidesList)
{
  // From issue #218
  const char* src =
    "class Named\n"
    "class Dog is Named\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "can only provide traits and interfaces");
}

TEST_F(BadPonyTest, TypeParamMissingForTypeInProvidesList)
{
  // From issue #219
  const char* src =
    "trait Bar[A]\n"
    "  fun bar(a: A) =>\n"
    "    None\n"

    "trait Foo is Bar // here also should be a type argument, like Bar[U8]\n"
    "  fun foo() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "not enough type arguments");
}

TEST_F(BadPonyTest, TupleIndexIsZero)
{
  // From issue #397
  const char* src =
    "primitive Foo\n"
    "  fun bar(): None =>\n"
    "    (None, None)._0";

  TEST_ERRORS_1(src, "Did you mean _1?");
}

TEST_F(BadPonyTest, TupleIndexIsOutOfRange)
{
  // From issue #397
  const char* src =
    "primitive Foo\n"
    "  fun bar(): None =>\n"
    "    (None, None)._3";

  TEST_ERRORS_1(src, "Valid range is [1, 2]");
}

TEST_F(BadPonyTest, InvalidLambdaReturnType)
{
  // From issue #828
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    {(): tag => this }\n";

  TEST_ERRORS_1(src, "lambda return type: tag");
}

TEST_F(BadPonyTest, InvalidMethodReturnType)
{
  // From issue #828
  const char* src =
    "primitive Foo\n"
    "  fun bar(): iso =>\n"
    "    U32(1)\n";

  TEST_ERRORS_1(src, "function return type: iso");
}

TEST_F(BadPonyTest, ObjectLiteralUninitializedField)
{
  // From issue #879
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    object\n"
    "      let x: I32\n"
    "    end";

  TEST_ERRORS_1(src, "object literal fields must be initialized");
}

TEST_F(BadPonyTest, LambdaCaptureVariableBeforeDeclarationWithTypeInferenceExpressionFail)
{
  // From issue #1018
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x) => None }\n"
    "    let x = 0";

   TEST_ERRORS_1(src, "declaration of 'x' appears after use");
}

// TODO: This test is not correct because it does not fail without the fix.
// I do not know how to generate a test that calls genheader().
// Comments are welcomed.
TEST_F(BadPonyTest, ExportedActorWithVariadicReturnTypeContainingNone)
{
  // From issue #891
  const char* src =
    "primitive T\n"
    "\n"
    "actor @A\n"
    "  fun f(a: T): (T | None) =>\n"
    "    a\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, TypeAliasRecursionThroughTypeParameterInTuple)
{
  // From issue #901
  const char* src =
    "type Foo is (Map[Foo, Foo], None)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "type aliases can't be recursive");
}

TEST_F(BadPonyTest, ParenthesisedReturn)
{
  // From issue #1050
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (return)";

  TEST_ERRORS_1(src, "use return only to exit early from a method");
}

TEST_F(BadPonyTest, ParenthesisedReturn2)
{
  // From issue #1050
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  fun foo(): U64 =>\n"
    "    (return 0)\n"
    "    2";

  TEST_ERRORS_1(src, "Unreachable code");
}

TEST_F(BadPonyTest, MatchUncalledMethod)
{
  // From issue #903
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match foo\n"
    "    | None => None\n"
    "    end\n"

    "  fun foo() =>\n"
    "    None";

  TEST_ERRORS_2(src, "can't reference a method without calling it",
                     "this pattern can never match");
}

TEST_F(BadPonyTest, TupleFieldReassign)
{
  // From issue #1101
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var foo: (U64, String) = (42, \"foo\")\n"
    "    foo._2 = \"bar\"";

  TEST_ERRORS_1(src, "can't assign to an element of a tuple");
}

TEST_F(BadPonyTest, WithBlockTypeInference)
{
  // From issue #1135
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with x = 1 do None end";

  TEST_ERRORS_3(src, "could not infer literal type, no valid types found",
                     "cannot infer type of $1$0",
                     "cannot infer type of x");
}

TEST_F(BadPonyTest, EmbedNestedTuple)
{
  // From issue #1136
  const char* src =
    "class Foo\n"
    "  fun get_foo(): Foo => Foo\n"

    "actor Main\n"
    "  embed foo: Foo\n"
    "  let x: U64\n"

    "  new create(env: Env) =>\n"
    "    (foo, x) = (Foo.get_foo(), 42)";

  TEST_ERRORS_1(src, "an embedded field must be assigned using a constructor");
}

TEST_F(BadPonyTest, CircularTypeInfer)
{
  // From issue #1334
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = x.create()\n"
        "let y = y.create()";

  TEST_ERRORS_2(src,
    "can't use an undefined variable in an expression",
    "can't use an undefined variable in an expression");
}

TEST_F(BadPonyTest, CallConstructorOnTypeIntersection)
{
  // From issue #1398
  const char* src =
    "interface Foo\n"

    "type Isect is (None & Foo)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Isect.create()";

  TEST_ERRORS_1(src, "can't call a constructor on a type intersection");
}

TEST_F(BadPonyTest, AssignToFieldOfIso)
{
  // From issue #1469
  const char* src =
    "class Foo\n"
    "  var x: String ref = String\n"
    "  fun iso bar(): String iso^ =>\n"
    "    let s = recover String end\n"
    "    x = s\n"
    "   consume s\n"

    "  fun ref foo(): String iso^ =>\n"
    "    let s = recover String end\n"
    "    let y: Foo iso = Foo\n"
    "    y.x = s\n"
    "    consume s";

  TEST_ERRORS_2(src,
    "right side must be a subtype of left side",
    "right side must be a subtype of left side"
    );
}

TEST_F(BadPonyTest, IndexArrayWithBrackets)
{
  // From issue #1493
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let xs = [as I64: 1; 2; 3]\n"
        "xs[1]";

  TEST_ERRORS_1(src, "Value formal parameters not yet supported");
}

TEST_F(BadPonyTest, ShadowingBuiltinTypeParameter)
{
  const char* src =
    "class A[I8]\n"
      "let b: U8 = 0";

  TEST_ERRORS_1(src, "type parameter shadows existing type");
}

TEST_F(BadPonyTest, ShadowingTypeParameterInSameFile)
{
  const char* src =
    "trait B\n"
    "class A[B]";

  TEST_ERRORS_1(src, "can't reuse name 'B'");
}

TEST_F(BadPonyTest, TupleToUnionGentrace)
{
  // From issue #1561
  const char* src =
    "primitive X\n"
    "primitive Y\n"

    "class iso T\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    this((T, Y))\n"

    "  be apply(m: (X | (T, Y))) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, RefCapViolationViaCapReadTypeParameter)
{
  // From issue #1328
  const char* src =
    "class Foo\n"
      "var i: USize = 0\n"
      "fun ref boom() => i = 3\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let a: Foo val = Foo\n"
        "call_boom[Foo val](a)\n"

      "fun call_boom[A: Foo #read](x: A) =>\n"
        "x.boom()";

  TEST_ERRORS_1(src, "receiver type is not a subtype of target type");
}

TEST_F(BadPonyTest, RefCapViolationViaCapAnyTypeParameter)
{
  // From issue #1328
  const char* src =
    "class Foo\n"
      "var i: USize = 0\n"
      "fun ref boom() => i = 3\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let a: Foo val = Foo\n"
        "call_boom[Foo val](a)\n"

      "fun call_boom[A: Foo #any](x: A) =>\n"
        "x.boom()";

  TEST_ERRORS_1(src, "receiver type is not a subtype of target type");
}

TEST_F(BadPonyTest, TypeParamArrowClass)
{
  // From issue #1687
  const char* src =
    "class C1\n"

    "trait Test[A]\n"
      "fun foo(a: A): A->C1";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, ArrowTypeParamInTypeConstraint)
{
  // From issue #1694
  const char* src =
    "trait T1[A: B->A, B]\n"
    "trait T2[A: box->B, B]";

  TEST_ERRORS_2(src,
    "arrow types can't be used as type constraints",
    "arrow types can't be used as type constraints");
}

TEST_F(BadPonyTest, ArrowTypeParamInMethodConstraint)
{
  // From issue #1809
  const char* src =
    "class Foo\n"
    "  fun foo[X: box->Y, Y](x: X) => None";

  TEST_ERRORS_1(src,
    "arrow types can't be used as type constraints");
}

TEST_F(BadPonyTest, AnnotatedIfClause)
{
  // From issue #1751
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if \\likely\\ U32(1) == 1 then\n"
    "      None\n"
    "    end\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CapSubtypeInConstrainSubtyping)
{
  // From PR #1816
  const char* src =
    "trait T\n"
    "  fun alias[X: Any iso](x: X!): X^\n"
    "class C is T\n"
    "  fun alias[X: Any tag](x: X!): X^ => x\n";

  TEST_ERRORS_1(src,
    "type does not implement its provides list");
}

TEST_F(BadPonyTest, ObjectInheritsLaterTraitMethodWithParameter)
{
  // From issue #1715
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    object is T end\n"

    "trait T\n"
    "  fun apply(n: I32): Bool =>\n"
    "    n == 0\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AddressofMissingTypearg)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo[None](addressof fn)\n"

    "  fun fn[A]() => None";

  TEST_ERRORS_1(src,
    "not enough type arguments");
}

TEST_F(BadPonyTest, ThisDotFieldRef)
{
  // From issue #1865
  const char* src =
    "actor Main\n"
    "  let f: U8\n"
    "  new create(env: Env) =>\n"
    "    this.f = 1\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CapSetInConstraintTypeParam)
{
  const char* src =
    "class A[X]\n"
    "class B[X: A[Any #read]]\n";

  TEST_ERRORS_1(src,
    "a capability set can only appear in a type constraint");
}

TEST_F(BadPonyTest, MatchCasePatternConstructorTooFewArguments)
{
  const char* src =
    "class C\n"
    "  new create(key: String) => None\n"

    "primitive Foo\n"
    "  fun apply(c: (C | None)) =>\n"
    "    match c\n"
    "    | C => None\n"
    "    end";

  TEST_ERRORS_1(src, "not enough arguments");
}

TEST_F(BadPonyTest, ThisDotWhereDefIsntInTheTrait)
{
  // From issue #1878
  const char* src =
    "trait T\n"
    "  fun foo(): USize => this.u\n"

    "class C is T\n"
    "  var u: USize = 0\n";

  TEST_ERRORS_1(src,
    "can't find declaration of 'u'");
}

TEST_F(BadPonyTest, DontCareTypeInTupleTypeOfIfBlockValueUnused)
{
  // From issue #1896
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true then\n"
    "      (var a, let _) = test()\n"
    "    end\n"
    "  fun test(): (U32, U32) =>\n"
    "    (1, 2)\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, ExhaustiveMatchCasesJumpAway)
{
  // From issue #1898
  const char* src =
    "primitive Foo\n"
    "  fun apply(b: Bool) =>\n"
    "    if true then\n"
    "      match b\n"
    "      | let b': Bool => return\n"
    "      end\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CallArgTypeErrorInsideTuple)
{
  // From issue #1895
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (\"\", foo([\"\"]))\n"
    "  fun foo(x: Array[USize]) => None";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(BadPonyTest, NonExistFieldReferenceInConstructor)
{
  // From issue #1932
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    this.x = None";

  TEST_ERRORS_2(src,
    "can't find declaration of 'x'",
    "left side must be something that can be assigned to");
}

TEST_F(BadPonyTest, TypeArgErrorInsideReturn)
{
  const char* src =
    "primitive P[A]\n"

    "primitive Foo\n"
    "  fun apply(): (P[None], U8) =>\n"
    "    if true then\n"
    "      return (P, 0)\n"
    "    end\n"
    "    (P[None], 1)";

  TEST_ERRORS_1(src, "not enough type arguments");
}

TEST_F(BadPonyTest, FieldReferenceInDefaultArgument)
{
  const char* src =
    "actor Main\n"
    "  let _env: Env\n"
    "  new create(env: Env) =>\n"
    "    _env = env\n"
    "    foo()\n"
    "  fun foo(env: Env = _env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "can't reference 'this' in a default argument");
}

TEST_F(BadPonyTest, DefaultArgScope)
{
  const char* src =
    "actor A\n"
    "  fun foo(x: None = (let y = None; y)) =>\n"
    "    y";

  TEST_ERRORS_1(src, "can't find declaration of 'y'");
}

TEST_F(BadPonyTest, GenericMain)
{
  const char* src =
    "actor Main[X]\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "the Main actor cannot have type parameters");
}

TEST_F(BadPonyTest, LambdaParamNoType)
{
  // From issue #2010
  const char* src =
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    {(x: USize, y): USize => x }";

  TEST_ERRORS_1(src, "a lambda parameter must specify a type");
}

TEST_F(BadPonyTest, AsBadCap)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let v: I64 = 3\n"
    "    try (v as I64 iso) end";

  TEST_ERRORS_1(src, "this capture violates capabilities");
}

TEST_F(BadPonyTest, AsUnaliased)
{
  const char* src =
    "class trn Foo\n"

    "actor Main\n"
    "  let foo: Foo = Foo\n"

    "  new create(env: Env) => None\n"

    "  fun ref apply() ? =>\n"
    "    (foo as Foo trn)";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CodegenMangledFunptr)
{
  // Test that we don't crash in codegen when generating the function pointer.
  const char* src =
    "interface I\n"
    "  fun foo(x: U32)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let i: I = this\n"
    "    @foo[None](addressof i.foo)\n"

    "  fun foo(x: Any) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AsFromUninferredReference)
{
  // From issue #2035
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let b = apply(true)\n"
    "    b as Bool\n"

    "  fun ref apply(s: String): (Bool | None) =>\n"
    "    true";

  TEST_ERRORS_2(src,
    "argument not a subtype of parameter",
    "cannot infer type of b");
}

TEST_F(BadPonyTest, FFIDeclaredTupleArgument)
{
  const char* src =
    "use @foo[None](x: (U8, U8))\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo((0, 0))";

  TEST_ERRORS_1(src, "cannot pass tuples as FFI arguments");
}

TEST_F(BadPonyTest, FFIUndeclaredTupleArgument)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo[None]((U8(0), U8(0)))";

  TEST_ERRORS_1(src, "cannot pass tuples as FFI arguments");
}

TEST_F(BadPonyTest, FFIDeclaredTupleReturn)
{
  const char* src =
    "use @foo[(U8, U8)]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo()";

  TEST_ERRORS_1(src, "an FFI function cannot return a tuple");
}

TEST_F(BadPonyTest, FFIUndeclaredTupleReturn)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo[(U8, U8)]()";

  TEST_ERRORS_1(src, "an FFI function cannot return a tuple");
}

TEST_F(BadPonyTest, MatchExhaustiveLastCaseUnionSubset)
{
  // From issue #2048
  const char* src =
    "primitive P1\n"
    "primitive P2\n"

    "actor Main\n"
    "  new create(env: Env) => apply(None)\n"

    "  fun apply(p': (P1 | P2 | None)) =>\n"
    "    match p'\n"
    "    | None => None\n"
    "    | let p: (P1 | P2) => None\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, QuoteCommentsEmptyLine)
{
  // From issue #2027
  const char* src =
    "\"\"\"\n"
    "\"\"\"\n"
    "actor Main\n"
    "  new create(env: Env) => None\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AsUninferredNumericLiteral)
{
  // From issue #2037
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    0 as I64\n"
    "    0.0 as F64";

  TEST_ERRORS_2(src,
    "Cannot cast uninferred numeric literal",
    "Cannot cast uninferred numeric literal");
}

TEST_F(BadPonyTest, AsNestedUninferredNumericLiteral)
{
  // From issue #2037
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (0, 1) as (I64, I64)";

  TEST_ERRORS_1(src, "Cannot cast uninferred literal");
}

TEST_F(BadPonyTest, DontCareUnusedBranchValue)
{
  // From issue #2073
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if false then\n"
    "      None\n"
    "    else\n"
    "      _\n"
    "    end";

  TEST_COMPILE(src);
}
