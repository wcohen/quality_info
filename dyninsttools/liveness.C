/*
 * liveness.C
 *
 * liveness is an attempt to get some information about which register
 * are live at various places in the binary.
 *
 * This code was derived from the Ben Woodard's dwqual/linemap.C
 * the dyninst dataflowAPI documentation liveness Analysis example.
 */

#include <dyninst/CodeSource.h>
#include <dyninst/Location.h>
#include <dyninst/liveness.h>
#include <dyninst/bitArray.h>

using namespace std;
using namespace Dyninst;
using namespace ParseAPI;

enum exit_codes {
		 EXIT_OK=0,
		 EXIT_ARGS=1,
		 EXIT_MODULE=2,
		 EXIT_NOFUNCS=3,
		 EXIT_LFUNC=4,
		 EXIT_LF_NOTUNIQ=5,
		 EXIT_GLOBALS=6,
		 EXIT_NOFILE=7
};

ostream *errfile;

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  errfile=&cerr;
  SymtabCodeSource *sts;
  CodeObject *co;

  file=argv[1];
  if (file.length() == 0) {
	  exit(EXIT_NOFILE);
  }

  // Create a new binary code object from the filename argument
  sts = new SymtabCodeSource( argv[1] );
  if( !sts )
    exit(EXIT_MODULE);
  co = new CodeObject( sts );
  if( !co )
    exit(EXIT_MODULE);

  // Parse the binary
  co->parse();

  #if 0
  // turn on the liveness analysis debugging output
  extern int dyn_debug_liveness;
  dyn_debug_liveness = 1;
  #endif


  //iterate through each of the the functions
  for( auto f: co->funcs()) {
	  LivenessAnalyzer la(f->obj()->cs()->getAddressWidth());

	  printf("# %s\n", f->name().c_str());
	  la.analyze(f);

	  printf ("# %s,%s,%s\n", "address", "in", "out");
	  // go through each block
	  for (auto b = f->blocks().begin(); b != f->blocks().end(); ++b) {
		  Block *bb = *b;
		  Block::Insns insns;

		  printf("# bb start %p end %p\n", bb->start(),  bb->last());

		  bb->getInsns(insns);
		  // go through each instruction in the block
		  for (auto insn_iter : insns) {
			  Instruction curInsn = insn_iter.second;
			  Address curAddr = insn_iter.first;

			  // construct a liveness query location
			  InsnLoc i(bb, curAddr, curInsn);
			  Location loc(f, i);
			  
			  // Query live registers at instruction entry
			  bitArray liveEntry;
			  if (!la.query(loc, LivenessAnalyzer::Before, liveEntry)) {
				  printf("Cannot look up live registers at instruction entry\n");
			  }

			  // Query live registers at instruction exit
			  bitArray liveExit;
			  if (!la.query(loc, LivenessAnalyzer::After, liveExit)) {
				  printf("Cannot look up live registers at instruction exit\n");
			  }

			  printf ("%p,%d,%d\n", curAddr, liveEntry.count(), liveExit.count());
		  }
	  }
  }
}
