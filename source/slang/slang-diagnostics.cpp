// slang-diagnostics.cpp
#include "slang-diagnostics.h"

#include "../core/slang-memory-arena.h"
#include "../core/slang-dictionary.h"
#include "../core/slang-string-util.h"

#include "../compiler-core/slang-name.h"
#include "../compiler-core/slang-core-diagnostics.h"

namespace Slang
{

namespace Diagnostics
{
#define DIAGNOSTIC(id, severity, name, messageFormat) const DiagnosticInfo name = { id, Severity::severity, #name, messageFormat };
#include "slang-diagnostic-defs.h"
#undef DIAGNOSTIC
}

static const DiagnosticInfo* const kAllDiagnostics[] =
{
#define DIAGNOSTIC(id, severity, name, messageFormat) &Diagnostics::name, 
#include "slang-diagnostic-defs.h"
#undef DIAGNOSTIC
};

static DiagnosticsLookup* _createLookup()
{
    DiagnosticsLookup* lookup = new DiagnosticsLookup(kAllDiagnostics, SLANG_COUNT_OF(kAllDiagnostics));

    // Add all the diagnostics in 'core'
    DiagnosticsLookup* coreLookup = getCoreDiagnosticsLookup();
    if (coreLookup)
    {
        for (auto diagnostic : coreLookup->getDiagnostics())
        {
            lookup->add(diagnostic);
        }
    }

    // Add the alias
    lookup->addAlias("overlappingBindings", "parameterBindingsOverlap");
    return lookup;
}

static DiagnosticsLookup* _getDiagnosticLookupSingleton()
{
    static RefPtr<DiagnosticsLookup> s_lookup = _createLookup();
    return s_lookup;
}

DiagnosticInfo const* findDiagnosticByName(UnownedStringSlice const& name)
{
    return _getDiagnosticLookupSingleton()->findDiagostic(name);
}

} // namespace Slang
