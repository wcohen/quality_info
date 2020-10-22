/*
 * locrangechecks.C
 *
 * locrangechecks is an attempt to do some simple verification of the
 * variable location ranges
 *
 * This code was derived from the Ben Woodard's dwqual/linemap.C
 * the dyninst dataflowAPI documentation liveness Analysis example.
 */

#include <dyninst/Symtab.h>
#include <dyninst/CodeSource.h>
#include <dyninst/Location.h>
#include <dyninst/Function.h>

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;
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

void output_entry(localVar *j, VariableLocation k)
{
	printf("%s [%x, %x] ", j->getName().c_str(), k.lowPC, k.hiPC);
	switch (k.stClass){
	case storageAddr:
		switch (k.refClass) {
		case storageRef: printf("*(%d)", k.frameOffset); break;
		case storageNoRef: printf("(%d)", k.frameOffset); break;
		}
		break;
	case storageReg:
		switch (k.refClass) {
		case storageRef: printf("(%s)", k.mr_reg.name().c_str()); break;
		case storageNoRef: printf("%s", k.mr_reg.name().c_str()); break;
		}
		break;
	case storageRegOffset:
		switch (k.refClass) {
		case storageRef: printf("*%d(%s)", k.frameOffset, k.mr_reg.name().c_str()); break;
		case storageNoRef: printf("%d(%s)", k.frameOffset, k.mr_reg.name().c_str()); break;
		}
		break;
	}
	printf("\n");
}

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  errfile=&cerr;
  SymtabCodeSource *sts = NULL;
  CodeObject *co = NULL;
  Symtab *syms = NULL;

  file=argv[1];
  if (file.length() == 0) {
	  exit(EXIT_NOFILE);
  }

  // Create a new binary code object from the filename argument
  sts = new SymtabCodeSource(argv[1]);
  if( !sts )
    exit(EXIT_MODULE);
  co = new CodeObject( sts );
  if( !co )
    exit(EXIT_MODULE);
  if(!Symtab::openFile(syms, argv[1]))
    exit(EXIT_MODULE);

  // Parse the binary
  co->parse();

  //iterate through each of the the functions
  for( auto f: co->funcs()) {
	  // List of all instruction in the function
	  Block::Insns func_insns;

	  // skip <name>.cold regions of code
	  if (f->name().find(".cold")!=string::npos)
		  continue;

	  // go through each basic block
	  for (auto bb : f->blocks()) {
		  Block::Insns insns;

		  // Add instruction in basic block to list of instructions
		  bb->getInsns(insns);
		  for (auto insn_iter : insns) {
			  func_insns.insert(insn_iter);
		  }
	  }
	  SymtabAPI::Function *func_sym;
	  if (!syms->findFuncByEntryOffset(func_sym, f->addr())) {
		  cerr << "unable to find " << f->name() << endl;
		  continue;
	  }
	  // Also make an entry for the end of the function
	  InstructionAPI::Instruction nothing;
	  Address func_start = f->addr();
	  Address func_end = func_start + func_sym->getSize();
	  func_insns[func_end] = nothing;

	  printf ("# %s [%x,%x]\n",
		  f->name().c_str(), func_start, func_end);

	  // now get list of locations for function
	  vector <localVar *> lvars;
	  func_sym->getParams(lvars);
	  func_sym->getLocalVariables(lvars);
	  for(auto j: lvars) {
	         vector<VariableLocation> &lvlocs=j->getLocationLists();
		 for(auto k: lvlocs) {
			 // Dyninst lumps .cold, other variants, and
			 // the regular func info together.
			 // just focus on the elements that have something
			 // within the bounds of the this func.
			 bool lowinrange = func_start <= k.lowPC && k.lowPC <= func_end;
			 bool hiinrange = func_start <= k.hiPC && k.hiPC <= func_end;
			 if (  !lowinrange && !hiinrange)
				 continue;
			 if (k.lowPC == 0 || k.hiPC ==-1){
				 // global variable always valid
				 // not going to be concerned about that.
				 continue;
			 }
			 output_entry(j, k);
			 #if 0
			 // Cannot do these checks because .cold and other
			 // disjoint regions of code grouped in same symbol
			 // check that each location region is reasonable
			 // 1) begin and end within function or global (don't span function boundaries)
			 if (k.lowPC < func_start || k.lowPC > func_end) {
				 printf ("name = %s, k.lowPC=%p outside function\n",
					 j->getName().c_str(), k.lowPC);
			 }
			 if (k.hiPC < func_start || k.hiPC > func_end) {
				 printf ("name = %s, k.hiPC=%p outside function\n",
					 j->getName().c_str(), k.hiPC);
			 }
			 
			 // 2) begin and end are on instruction boundaries
			 if (func_insns.find(k.lowPC) == func_insns.end()) {
				 printf ("name = %s, k.lowPC=%p not valid boundary\n",
				 j->getName().c_str(), k.lowPC);
			 }
			 if (func_insns.find(k.hiPC) == func_insns.end()) {
				 printf ("name = %s, k.hiPC=%p not valid boundary\n",
				 j->getName().c_str(), k.hiPC);
			 }
			 #endif
			 // 3) boundaries  match up with a read or a write operations
			 // use the dyninst instructionAPI to determine the operations
			 // use the dyninst symtabAPI to determine where location is
		 }
	  }
  }
}
