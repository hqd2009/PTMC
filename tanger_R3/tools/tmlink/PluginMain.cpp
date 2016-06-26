#include "llvm/CompilerDriver/BuiltinOptions.h"
#include "llvm/CompilerDriver/CompilationGraph.h"
#include "llvm/CompilerDriver/Tool.h"
#include "llvm/CompilerDriver/Error.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/LLVMContext.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <stdexcept>

using namespace llvm;
using namespace llvmc;

void transform(const char* infile, const char* outfile);

class real_llvm_tm_linker : public Tool {
private:
    static const char* InputLanguages_[];

public:
    const char* Name() const {
        return "llvm_tm_linker";
    }

    const char** InputLanguages() const {
        return InputLanguages_;
    }

    const char* OutputLanguage() const {
        return "llvm-bitcode";
    }

    bool IsJoin() const {
        return false;
    }

    bool WorksOnEmpty() const {
        return false;
    }

    int GenerateAction(Action& Out, const PathVector& inFiles,
        bool HasChildren,
        const llvm::sys::Path& TempDir,
        const InputLanguagesSet& InLangs,
        const LanguageMap& LangMap) const
    {
        PrintError("llvm_tm_linker is not a Join tool!");
        return 1;
    }

    int GenerateAction(Action& Out, const sys::Path& inFile,
        bool HasChildren,
        const llvm::sys::Path& TempDir,
        const InputLanguagesSet& InLangs,
        const LanguageMap& LangMap) const
    {
        std::string cmd;
        std::string out_file;
        std::vector<std::pair<unsigned, std::string> > vec;
        bool stop_compilation = !HasChildren;
        const char* output_suffix = "bc";

        cmd = "echo";
        vec.push_back(std::make_pair(0, "executed tmtransform"));

        vec.push_back(std::make_pair(InputFilenames.getPosition(0), inFile.str()));
        vec.push_back(std::make_pair(65536, "-o"));
        out_file = this->OutFilename(inFile,
                TempDir, stop_compilation, output_suffix).str();

        vec.push_back(std::make_pair(65536, out_file));

        transform(inFile.c_str(), out_file.c_str());

        Out.Construct(cmd, this->SortArgs(vec), stop_compilation, out_file);
        return 0;
    }

};
const char* real_llvm_tm_linker::InputLanguages_[] = {"llvm-bitcode", 0};

int RealPopulateCompilationGraphLocal(CompilationGraph& G);

// rewrite compilation graph
#define PopulateCompilationGraph PopulateCompilationGraph(CompilationGraph& graph) \
        { return RealPopulateCompilationGraphLocal(graph); } \
        int dummyIgnore
#include "TMLink.inc"

int RealPopulateCompilationGraphLocal(CompilationGraph& G) {
    G.insertNode(new llvm_link_optimize());
    G.insertNode(new llvm_link_together());
    G.insertNode(new llvm_tm_assembler());
    G.insertNode(new llvm_tm_compiler());
    G.insertNode(new real_llvm_tm_linker());
    G.insertNode(new llvm_tm_stm_support());

    if (int ret = G.insertEdge("root", new SimpleEdge("llvm_link_together")))
        return ret;
    if (int ret = G.insertEdge("llvm_link_together", new Edge1()))
        return ret;
    if (int ret = G.insertEdge("llvm_link_together", new SimpleEdge("llvm_tm_linker")))
        return ret;
    if (int ret = G.insertEdge("llvm_link_optimize", new SimpleEdge("llvm_tm_linker")))
        return ret;
    if (int ret = G.insertEdge("llvm_tm_linker", new SimpleEdge("llvm_tm_stm_support")))
        return ret;
    if (int ret = G.insertEdge("llvm_tm_stm_support", new SimpleEdge("llvm_tm_compiler")))
        return ret;
    if (int ret = G.insertEdge("llvm_tm_compiler", new SimpleEdge("llvm_tm_assembler")))
        return ret;

    return 0;
}

namespace hooks {
  std::string STMLIBDIR(){
    return autogenerated::Parameter_stmlib;
  }

  std::string STMSUPPORTDIR(){
    return autogenerated::Parameter_stmsupport;
  }
}


//
// Print config
//
struct ConfigParser : public cl::basic_parser<unsigned> {
    // parse - Return true on error.
    bool parse(cl::Option &O, StringRef ArgName, StringRef Arg, unsigned &Val)
    {
#ifdef ENABLE_DSA
        if (Arg == "ENABLE_DSA") outs() << "ENABLE_DSA=1\n";
#endif
#ifdef ENABLE_REPORTING
        if (Arg == "ENABLE_REPORTING") outs() << "ENABLE_REPORTING=1\n";
#endif
        return false;
    }
};

static cl::opt<unsigned, false, ConfigParser>
  PF("print-config", cl::desc("Print config flag (if enabled)"), cl::Hidden,
      cl::value_desc("flag"));


//===-- Based on: pa - Automatic Pool Allocation Compiler Tool ----------===//
//
//                     Automatic Pool Allocation Project
//
// This file was developed by the LLVM research group and is distributed
// under the University of Illinois Open Source License. See LICENSE.TXT for
// details.
//
//===--------------------------------------------------------------------===//

#include "../lib/tanger/tanger.h"
#include "llvm/Config/config.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/System/Signals.h"
#include "llvm/Support/raw_os_ostream.h"

#include <fstream>
#include <iostream>
#include <memory>

class PRL : public PassRegistrationListener {
    Pass* pass;
    std::string pName;
public:
    Pass* getPass(std::string passName) {
        pass = 0;
        pName = passName;
        enumeratePasses();
        return pass;
    }
    virtual void passEnumerate(const PassInfo* pi)
    {
        if (pName == pi->getPassArgument()) pass = pi->createPass();
    }
};

void transform(const char* infile, const char* outfile)
{
    std::string InputFilename(infile);
    std::string OutputFilename(outfile);

    // Load the module to be compiled...
    std::auto_ptr<Module> M;
    std::string ErrorMessage;
    if (MemoryBuffer *Buffer
        = MemoryBuffer::getFileOrSTDIN(InputFilename, &ErrorMessage)) {
      M.reset(ParseBitcodeFile(Buffer, getGlobalContext(), &ErrorMessage));
      delete Buffer;
    }

    if (M.get() == 0) {
      errs() << "tmlink: bytecode didn't read correctly.\n";
      exit(1);
    }

    // Build up all of the passes that we want to do to the module...
    PassManager Passes;

    Passes.add(new TargetData(M.get()));
    Passes.add(createInternalizePass(true));
    Passes.add(createPromoteMemoryToRegisterPass());

    // TODO cmdline opt chooses pass
    PRL prl;
    Pass* pass = prl.getPass(autogenerated::Parameter_tangerpass);
    if (!pass) {
        errs() << "tmlink: cannot find pass \"" << autogenerated::Parameter_tangerpass << "\".\n";
        exit(1);
    }
    Passes.add(pass);

    // Verify the final result
    Passes.add(createVerifierPass());

    // Figure out where we are going to send the output...
    raw_ostream *Out = 0;
    std::string error;
    if (OutputFilename != "-") {
        // Specified an output filename?
        Out = new raw_fd_ostream (OutputFilename.c_str(), error);

        // Make sure that the Out file gets unlinked from the disk if we get a
        // SIGINT
        sys::RemoveFileOnSignal(sys::Path(OutputFilename));
    } else {
        Out = &outs();
    }

    // Add the writing of the output file to the list of passes
    Passes.add (createBitcodeWriterPass(*Out));

    // Run our queue of passes all at once now, efficiently.
    Passes.run(*M.get());

    // Delete the ostream
    delete Out;
}

#include "llvm/CompilerDriver/Main.inc"
