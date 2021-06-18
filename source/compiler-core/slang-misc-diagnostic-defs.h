//

// The file is meant to be included multiple times, to produce different
// pieces of declaration/definition code related to diagnostic messages
//
// Each diagnostic is declared here with:
//
//     DIAGNOSTIC(id, severity, name, messageFormat)
//
// Where `id` is the unique diagnostic ID, `severity` is the default
// severity (from the `Severity` enum), `name` is a name used to refer
// to this diagnostic from code, and `messageFormat` is the default
// (non-localized) message for the diagnostic, with placeholders
// for any arguments.

#ifndef DIAGNOSTIC
#error Need to #define DIAGNOSTIC(...) before including 
#define DIAGNOSTIC(id, severity, name, messageFormat) /* */
#endif

//
// -1 - Notes that decorate another diagnostic.
//

DIAGNOSTIC(-1, Note, seeTokenPasteLocation, "see token pasted location")

DIAGNOSTIC(100000, Error, downstreamNameNotKnown, "downstream tool name not known, allowed names are $0")
DIAGNOSTIC(100001, Error, expectedArgumentForOption, "expected an argument for command-line option '$0'")
DIAGNOSTIC(100002, Error, unbalancedDownstreamArguments, "unbalanced downstream arguments")
DIAGNOSTIC(100003, Error, closeOfUnopenDownstreamArgs, "close of an unopen downstream argument scope")
DIAGNOSTIC(100004, Error, downstreamToolNameNotDefined, "downstream tool name not defined")

DIAGNOSTIC(110001, Error, structTagConversionFailureNoArena, "StructTag conversion failed - no arena")
DIAGNOSTIC(110002, Error, unknownStructTag, "unknown StructTag $0")
DIAGNOSTIC(110003, Error, cannotConvertStructTag, "cannot convert StructTag $0 to $1")
DIAGNOSTIC(110004, Error, cannotConvertDifferentStructTag, "cannot convert different StructTag types $0 to $1")

DIAGNOSTIC(120005, Error, unknownStructTagCategory, "unknown StructTag category $0")
DIAGNOSTIC(120006, Error, structTagInconsistent, "StructTag $0 is inconsistently defined")

DIAGNOSTIC(99999, Note, noteLocationOfInternalError, "an internal error threw an exception while working on code near this location")

#undef DIAGNOSTIC
