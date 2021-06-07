/*
 * locrangechecks.C
 *
 * locrangechecks is an attempt to do some simple verification of the
 * variable location ranges
 *
 * This code was derived from the Ben Woodard's dwqual/linemap.C
 * the dyninst dataflowAPI documentation liveness Analysis example.
 */

#include <boost/icl/interval_map.hpp>

#include <dyninst/Symtab.h>
#include <dyninst/CodeSource.h>
#include <dyninst/Location.h>
#include <dyninst/Function.h>

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace boost::icl;

#include "common.h"

const char *argp_program_version = "locrangechecks 0.1";
const char *argp_program_bug_address = "https://github.com/wcohen/quality_info/issues";

/* Program documentation. */
char doc[] = "locrangechecks -- examine binary and debuginfo to do sanity checks on variable location information";

void output_entry(localVar *j, VariableLocation k)
{
	printf("%s [%p, %p] ", j->getName().c_str(), k.lowPC, k.hiPC);
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

exit_codes analyze_binaries(session_info &session)
{
  //iterate through each of the the functions
  for( auto f: session.co->funcs()) {
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
			  // Treat calls as special as valid to have range inside
			  if (insn_iter.second.getCategory() == InstructionAPI::c_CallInsn)
				  for(auto i=1; i<insn_iter.second.size(); ++i){
					  pair <const long unsigned int, Dyninst::InstructionAPI::Instruction> caller(i+insn_iter.first, insn_iter.second);
					  func_insns.insert(caller);
				  }
		  }
	  }
	  SymtabAPI::Function *func_sym;
	  if (!session.syms->findFuncByEntryOffset(func_sym, f->addr())) {
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
			 
			 // 2) begin on instruction boundaries
			 if (func_insns.find(k.lowPC) == func_insns.end()) {
				 printf ("name = %s, k.lowPC=%p not valid boundary\n",
				 j->getName().c_str(), k.lowPC);
			 }
			 // 2a) end on instruction boundaries
			 //     Calls are special. Need to distiguish before/during/afer the call.
			 //     (http://lists.dwarfstd.org/pipermail/dwarf-discuss-dwarfstd.org/2018-December/004509.html)
			 //     This is taken care of with the setup of list of valid end points
			 if (func_insns.find(k.hiPC) == func_insns.end()) {
				 printf ("name = %s, k.hiPC=%p not valid boundary\n",
				 j->getName().c_str(), k.hiPC);
			 }
			 // 3) boundaries  match up with a read or a write operations
			 // use the dyninst instructionAPI to determine the operations
			 // use the dyninst symtabAPI to determine where location is
		 }
	  }
  }
  return(EXIT_OK);
}

int main(int argc, char **argv){
  session_info session;

  exit_codes options_status = process_options(argc, argv, session);
  if ( options_status != EXIT_OK)
	  exit(options_status);

  exit_codes binaries_status = process_binaries(session);
  if ( binaries_status != EXIT_OK)
	  exit(binaries_status);

  exit_codes filters_status = process_filters(session);
  if ( filters_status != EXIT_OK)
	  exit(filters_status);

  exit_codes analyze_status = analyze_binaries(session);
  exit(analyze_status);
}
