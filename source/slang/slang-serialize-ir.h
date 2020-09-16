// slang-serialize-ir.h
#ifndef SLANG_SERIALIZE_IR_H_INCLUDED
#define SLANG_SERIALIZE_IR_H_INCLUDED

#include "slang-serialize-ir-types.h"

#include "../core/slang-riff.h"

#include "slang-ir.h"
#include "slang-serialize-debug.h"

// For TranslationUnitRequest
// and FrontEndCompileRequest::ExtraEntryPointInfo
#include "slang-compiler.h"

namespace Slang {

struct IRSerialWriter
{
    typedef IRSerialData Ser;
    typedef IRSerialBinary Bin;

    Result write(IRModule* module, DebugSerialWriter* debugWriter, SerialOptionFlags flags, IRSerialData* serialData);
  
        /// Write to a container
    static Result writeContainer(const IRSerialData& data, SerialCompressionType compressionType, RiffContainer* container);
    
    /// Get an instruction index from an instruction
    Ser::InstIndex getInstIndex(IRInst* inst) const { return inst ? Ser::InstIndex(m_instMap[inst]) : Ser::InstIndex(0); }

        /// Get a slice from an index
    UnownedStringSlice getStringSlice(Ser::StringIndex index) const { return m_stringSlicePool.getSlice(StringSlicePool::Handle(index)); }
        /// Get index from string representations
    Ser::StringIndex getStringIndex(StringRepresentation* string) { return Ser::StringIndex(m_stringSlicePool.add(string)); }
    Ser::StringIndex getStringIndex(const UnownedStringSlice& slice) { return Ser::StringIndex(m_stringSlicePool.add(slice)); }
    Ser::StringIndex getStringIndex(Name* name) { return name ? getStringIndex(name->text) : SerialStringData::kNullStringIndex; }
    Ser::StringIndex getStringIndex(const char* chars) { return Ser::StringIndex(m_stringSlicePool.add(chars)); }
    Ser::StringIndex getStringIndex(const String& string) { return Ser::StringIndex(m_stringSlicePool.add(string.getUnownedSlice())); }

    StringSlicePool& getStringPool() { return m_stringSlicePool;  }
    
    IRSerialWriter() :
        m_serialData(nullptr),
        m_stringSlicePool(StringSlicePool::Style::Default)
    {
    }

        /// Produces an instruction list which is in same order as written through IRSerialWriter
    static void calcInstructionList(IRModule* module, List<IRInst*>& instsOut);

protected:
    
    void _addInstruction(IRInst* inst);
    Result _calcDebugInfo(DebugSerialWriter* debugWriter);
    
    List<IRInst*> m_insts;                              ///< Instructions in same order as stored in the 

    List<IRDecoration*> m_decorations;                  ///< Holds all decorations in order of the instructions as found
    List<IRInst*> m_instWithFirstDecoration;            ///< All decorations are held in this order after all the regular instructions

    Dictionary<IRInst*, Ser::InstIndex> m_instMap;      ///< Map an instruction to an instruction index

    StringSlicePool m_stringSlicePool;    
    IRSerialData* m_serialData;                         ///< Where the data is stored
};

struct IRSerialReader
{
    typedef IRSerialData Ser;
    typedef SerialStringTable::Handle StringHandle;

        /// Read potentially multiple modules from a stream
    static Result readStreamModules(Stream* stream, Session* session, SourceManager* manager, List<RefPtr<IRModule>>& outModules, List<FrontEndCompileRequest::ExtraEntryPointInfo>& outEntryPoints);

        /// Read a stream to fill in dataOut IRSerialData
    static Result readContainer(RiffContainer::ListChunk* module, SerialCompressionType containerCompressionType, IRSerialData* outData);

        /// Read a module from serial data
    Result read(const IRSerialData& data, Session* session, DebugSerialReader* debugReader, RefPtr<IRModule>& outModule);

    IRSerialReader():
        m_serialData(nullptr),
        m_module(nullptr)
    {
    }

    protected:

    SerialStringTable m_stringTable;

    const IRSerialData* m_serialData;
    IRModule* m_module;
};

} // namespace Slang

#endif
