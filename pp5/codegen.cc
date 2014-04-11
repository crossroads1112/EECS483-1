/* File: codegen.cc
 * ----------------
 * Implementation for the CodeGenerator class. The methods don't do anything
 * too fancy, mostly just create objects of the various Tac instruction
 * classes and append them to the list.
 */

#include "codegen.h"
#include <string.h>
#include "tac.h"
#include "mips.h"
#include "ast_decl.h"
#include "errors.h"
#include <vector>
#include <string>
  
using namespace std;
  
CodeGenerator::CodeGenerator()
{
  code = new List<Instruction*>();
  curGlobalOffset = 0;
}

void CodeGenerator::createCFG(int begin)
{
    BeginFunc* bf = dynamic_cast<BeginFunc*>(code->Nth(begin));
    Assert(bf); //always start at BeginFunc
    Goto* gt;
    IfZ* iz;
    EndFunc* ef;
    Return* ret;
    /*
    dynamic casts to paste in as needed
    LoadConstant* lc = dynamic_cast<LoadConstant*>(code->Nth(//XXX));
    LoadStringConstant* lcs = dynamic_cast<LoadStringConstant*>(code->Nth(//XXX));
    LoadLabel* ll = dynamic_cast<LoadLabel*>(code->Nth(//XXX));
    Assign* assign = dynamic_cast<Assign*>(code->Nth(//XXX));
    Load* load = dynamic_cast<Load*>(code->Nth(//XXX));
    Store* store = dynamic_cast<Store*>(code->Nth(//XXX));
    BinaryOp* bo = dynamic_cast<BinaryOp*>(code->Nth(//XXX));
    Label* label = dynamic_cast<Label*>(code->Nth(//XXX));
    Goto* gt = dynamic_cast<Goto*>(code->Nth(//XXX));
    IfZ* iz = dynamic_cast<IfZ*>(code->Nth(//XXX));
    EndFunc* ef = dynamic_cast<EndFunc*>(code->Nth(//XXX));
    Return* ret = dynamic_cast<Return*>(code->Nth(//XXX));
    PushParam* pp = dynamic_cast<PushParam*>(code->Nth(//XXX));
    RemoveParams* rp = dynamic_cast<RemoveParams*>(code->Nth(//XXX));
    LCall* lCall = dynamic_cast<LCall*>(code->Nth(//XXX));
    ACall* aCall = dynamic_cast<ACall*>(code->Nth(//XXX));
    VTable* vt = dynamic_cast<VTable*>(code->Nth(//XXX));
    */
    for (int i = begin; i < code->NumElements(); i++)
    {
        //If EndFunc, finished
        ef = dynamic_cast<EndFunc*>(code->Nth(i));
        if (ef)
            break;
            
        //If Goto, find label, add to edges
        gt = dynamic_cast<Goto*>(code->Nth(i));
        if (gt)
        {
            string s = gt->getLabel();
            gt->addEdge(labels[s]);
            continue;
        }
        
        //If IfZ, find label, add next instruction and label to edges
        iz = dynamic_cast<IfZ*>(code->Nth(i));
        if (iz)
        {
            string s = iz->getLabel();
            iz->addEdge(labels[s]);
            
            iz->addEdge(code->Nth(i+1));
            continue;
        }
        Return* ret = dynamic_cast<Return*>(code->Nth(i));
        if (ret)
        {
            continue;
        }
        code->Nth(i)->addEdge(code->Nth(i+1)); //if instruction doesnt fit any above, add next instruction
    }
    livenessAnalysis(begin);
}

//TODO confused by algorithm, talk to Chun
void CodeGenerator::livenessAnalysis(int begin)
{
    Instruction* instruction;
    Instruction* edge;

    BeginFunc* bf = dynamic_cast<BeginFunc*>(code->Nth(begin));
    Assert(bf);

    bool changed = true;
    while (changed)
    {
        changed = false;

        for (int i = begin; i < code->NumElements(); i++) //for each TAC in CFG:
        {
            List<string> outSet;
            instruction = code->Nth(i);
            for (int j = 0; j < instruction->getNumEdges(); j++) //Out[TAC] = Union(In[Succ(TAC)])
            {
                edge = instruction->getEdge(j);
                for (int k = 0; k < edge->inSet.NumElements(); k++) //Union every elem into outSet
                {
                    string elem = edge->inSet.Nth(k);
                    for (int l = 0; l < outSet.NumElements(); l++)  //check to see if elem is already in outSet
                    {
                        if (outSet.Nth(l) == elem)
                            break;
                        outSet.Append(elem); //if elem isn't already in outSet, add it
                    }
                }
            }

            //In'[TAC] = Out[TAC] - Kill[TAC] + Gen[TAC]
            List<string> inPrimeSet = outSet; //= Out[TAC]
            List<string> killSet = instruction->KillSet();
            List<string> genSet = instruction->GenSet();

            for (int j = 0; j < inPrimeSet.NumElements(); j++) //- Kill[TAC]
            {
                for (int k = 0; k < killSet.NumElements(); k++)
                {
                    if (inPrimeSet.Nth(j) == killSet.Nth(k))
                    {
                        inPrimeSet.RemoveAt(j);
                        j--; //so elements dont get skipped
                        break;
                    }
                }
            }
            for (int j = 0; j < genSet.NumElements(); j++) //+ Gen[TAC]
            {
                bool found = false;
                for (int k = 0; k < inPrimeSet.NumElements(); k++)
                {
                    if (inPrimeSet.Nth(k) == genSet.Nth(j))
                    {
                        found = true;
                        break;
                    }
                }
                
                if (!found)
                    inPrimeSet.Append(genSet.Nth(j));
            }
            //TODO rest of algorithm
            
            List<string> tempInPrimeSet = inPrimeSet; 
            
            bool foundInOutSet = false;
            bool setsAreDifferent = false;
             
            for(int i=0; i< outSet.NumElements(); i++)
            {
            	for(int j=0; j<tempInPrimeSet.NumElements(); j++)
            	{
            		if(outSet.Nth(i) == tempInPrimeSet.Nth(j))
            		{
            			tempInPrimeSet.RemoveAt(j); 
            			foundInOutSet = true; 
            			break;
            		}
            	}
            	
            	if(foundInOutSet)
            	{
            		foundInOutSet = false; 
            		continue; 
            	}
            	else
            	{
            		setsAreDifferent = true;
            		break;
            	}
            }            
            
            if(setsAreDifferent || tempInPrimeSet.NumElements() != 0)
            {
            	outSet = inPrimeSet; 
            	changed = true; 
            }
            
        }
    }
}

char *CodeGenerator::NewLabel()
{
  static int nextLabelNum = 0;
  char temp[10];
  sprintf(temp, "_L%d", nextLabelNum++);
  return strdup(temp);
}


Location *CodeGenerator::GenTempVar()
{
  static int nextTempNum;
  char temp[10];
  Location *result = NULL;
  sprintf(temp, "_tmp%d", nextTempNum++);
  return GenLocalVariable(temp);
}

  
Location *CodeGenerator::GenLocalVariable(const char *varName)
{            
    curStackOffset -= VarSize;
    return new Location(fpRelative, curStackOffset+4,  varName);
}

Location *CodeGenerator::GenGlobalVariable(const char *varName)
{
    curGlobalOffset += VarSize;
    return new Location(gpRelative, curGlobalOffset -4, varName);
}


Location *CodeGenerator::GenLoadConstant(int value)
{
  Location *result = GenTempVar();
  code->Append(new LoadConstant(result, value));
  return result;
}

Location *CodeGenerator::GenLoadConstant(const char *s)
{
  Location *result = GenTempVar();
  code->Append(new LoadStringConstant(result, s));
  return result;
} 

Location *CodeGenerator::GenLoadLabel(const char *label)
{
  Location *result = GenTempVar();
  code->Append(new LoadLabel(result, label));
  return result;
} 


void CodeGenerator::GenAssign(Location *dst, Location *src)
{
  code->Append(new Assign(dst, src));
}


Location *CodeGenerator::GenLoad(Location *ref, int offset)
{
  Location *result = GenTempVar();
  code->Append(new Load(result, ref, offset));
  return result;
}

void CodeGenerator::GenStore(Location *dst,Location *src, int offset)
{
  code->Append(new Store(dst, src, offset));
}


Location *CodeGenerator::GenBinaryOp(const char *opName, Location *op1,
						     Location *op2)
{
  Location *result = GenTempVar();
  code->Append(new BinaryOp(BinaryOp::OpCodeForName(opName), result, op1, op2));
  return result;
}


void CodeGenerator::GenLabel(const char *label)
{
  Instruction* instruction = new Label(label);
  code->Append(instruction);
  string s = label;
  labels[s] = instruction;
}

void CodeGenerator::GenIfZ(Location *test, const char *label)
{
  code->Append(new IfZ(test, label));
}

void CodeGenerator::GenGoto(const char *label)
{
  code->Append(new Goto(label));
}

void CodeGenerator::GenReturn(Location *val)
{
  code->Append(new Return(val));
}


BeginFunc *CodeGenerator::GenBeginFunc(FnDecl *fn)
{
  BeginFunc *result = new BeginFunc;
  code->Append(insideFn = result);
  List<VarDecl*> *formals = fn->GetFormals();
  int start = OffsetToFirstParam;
  if (fn->IsMethodDecl()) start += VarSize;
  for (int i = 0; i < formals->NumElements(); i++)
    formals->Nth(i)->rtLoc = new Location(fpRelative, i*VarSize + start, formals->Nth(i)->GetName());
  curStackOffset = OffsetToFirstLocal;
  return result;
}

void CodeGenerator::GenEndFunc()
{
  code->Append(new EndFunc());
  insideFn->SetFrameSize(OffsetToFirstLocal-curStackOffset);
  insideFn = NULL;
}

void CodeGenerator::GenPushParam(Location *param)
{
  code->Append(new PushParam(param));
}

void CodeGenerator::GenPopParams(int numBytesOfParams)
{
  Assert(numBytesOfParams >= 0 && numBytesOfParams % VarSize == 0); // sanity check
  if (numBytesOfParams > 0)
    code->Append(new PopParams(numBytesOfParams));
}

Location *CodeGenerator::GenLCall(const char *label, bool fnHasReturnValue)
{
  Location *result = fnHasReturnValue ? GenTempVar() : NULL;
  code->Append(new LCall(label, result));
  return result;
}
  
Location *CodeGenerator::GenFunctionCall(const char *fnLabel, List<Location*> *args, bool hasReturnValue)
{
  for (int i = args->NumElements()-1; i >= 0; i--) // push params right to left
    GenPushParam(args->Nth(i));
  Location *result = GenLCall(fnLabel, hasReturnValue);
  GenPopParams(args->NumElements()*VarSize);
  return result;
}

Location *CodeGenerator::GenACall(Location *fnAddr, bool fnHasReturnValue)
{
  Location *result = fnHasReturnValue ? GenTempVar() : NULL;
  code->Append(new ACall(fnAddr, result));
  return result;
}
  
Location *CodeGenerator::GenMethodCall(Location *rcvr,
			     Location *meth, List<Location*> *args, bool fnHasReturnValue)
{
  for (int i = args->NumElements()-1; i >= 0; i--)
    GenPushParam(args->Nth(i));
  GenPushParam(rcvr);	// hidden "this" parameter
  Location *result= GenACall(meth, fnHasReturnValue);
  GenPopParams((args->NumElements()+1)*VarSize);
  return result;
}
 
 
static struct _builtin {
  const char *label;
  int numArgs;
  bool hasReturn;
} builtins[] =
 {{"_Alloc", 1, true},
  {"_ReadLine", 0, true},
  {"_ReadInteger", 0, true},
  {"_StringEqual", 2, true},
  {"_PrintInt", 1, false},
  {"_PrintString", 1, false},
  {"_PrintBool", 1, false},
  {"_Halt", 0, false}};

Location *CodeGenerator::GenBuiltInCall(BuiltIn bn,Location *arg1, Location *arg2)
{
  Assert(bn >= 0 && bn < NumBuiltIns);
  struct _builtin *b = &builtins[bn];
  Location *result = NULL;

  if (b->hasReturn) result = GenTempVar();
                // verify appropriate number of non-NULL arguments given
  Assert((b->numArgs == 0 && !arg1 && !arg2)
	|| (b->numArgs == 1 && arg1 && !arg2)
	|| (b->numArgs == 2 && arg1 && arg2));
  if (arg2) code->Append(new PushParam(arg2));
  if (arg1) code->Append(new PushParam(arg1));
  code->Append(new LCall(b->label, result));
  GenPopParams(VarSize*b->numArgs);
  return result;
}


void CodeGenerator::GenVTable(const char *className, List<const char *> *methodLabels)
{
  code->Append(new VTable(className, methodLabels));
}


void CodeGenerator::DoFinalCodeGen()
{
  if (IsDebugOn("tac")) { // if debug don't translate to mips, just print Tac
    for (int i = 0; i < code->NumElements(); i++)
	code->Nth(i)->Print();
   }  else {
     Mips mips;
     mips.EmitPreamble();
     for (int i = 0; i < code->NumElements(); i++)
	 code->Nth(i)->Emit(&mips);
  }
}



Location *CodeGenerator::GenArrayLen(Location *array)
{
  return GenLoad(array, -4);
}

Location *CodeGenerator::GenNew(const char *vTableLabel, int instanceSize)
{
  Location *size = GenLoadConstant(instanceSize);
  Location *result = GenBuiltInCall(Alloc, size);
  Location *vt = GenLoadLabel(vTableLabel);
  GenStore(result, vt);
  return result;
}


Location *CodeGenerator::GenDynamicDispatch(Location *rcvr, int vtableOffset, List<Location*> *args, bool hasReturnValue)
{
  Location *vptr = GenLoad(rcvr); // load vptr
  Assert(vtableOffset >= 0);
  Location *m = GenLoad(vptr, vtableOffset*4);
  return GenMethodCall(rcvr, m, args, hasReturnValue);
}

// all variables (ints, bools, ptrs, arrays) are 4 bytes in for code generation
// so this simplifies the math for offsets
Location *CodeGenerator::GenSubscript(Location *array, Location *index)
{
  Location *zero = GenLoadConstant(0);
  Location *isNegative = GenBinaryOp("<", index, zero);
  Location *count = GenLoad(array, -4);
  Location *isWithinRange = GenBinaryOp("<", index, count);
  Location *pastEnd = GenBinaryOp("==", isWithinRange, zero);
  Location *outOfRange = GenBinaryOp("||", isNegative, pastEnd);
  const char *pastError = NewLabel();
  GenIfZ(outOfRange, pastError);
  GenHaltWithMessage(err_arr_out_of_bounds);
  GenLabel(pastError);
  Location *four = GenLoadConstant(VarSize);
  Location *offset = GenBinaryOp("*", four, index);
  Location *elem = GenBinaryOp("+", array, offset);
  return new Location(elem, 0); 
}



Location *CodeGenerator::GenNewArray(Location *numElems)
{
  Location *one = GenLoadConstant(1);
  Location *isNonpositive = GenBinaryOp("<", numElems, one);
  const char *pastError = NewLabel();
  GenIfZ(isNonpositive, pastError);
  GenHaltWithMessage(err_arr_bad_size);
  GenLabel(pastError);

  // need (numElems+1)*VarSize total bytes (extra 1 is for length)
  Location *arraySize = GenLoadConstant(1);
  Location *num = GenBinaryOp("+", arraySize, numElems);
  Location *four = GenLoadConstant(VarSize);
  Location *bytes = GenBinaryOp("*", num, four);
  Location *result = GenBuiltInCall(Alloc, bytes);
  GenStore(result, numElems);
  return GenBinaryOp("+", result, four);
}

void CodeGenerator::GenHaltWithMessage(const char *message)
{
   Location *msg = GenLoadConstant(message);
   GenBuiltInCall(PrintString, msg);
   GenBuiltInCall(Halt, NULL);
}