/****************************************************
 * file name:   engine.c
 * description:	java virtual machine executing engine
 * author:		Kari.Zhang
 * modifications:
 *		1. Created by Kari.zhang @ 2015-11-25
 *
 *		2. Update executeMethod() 
 *			 by kari.zhang @ 2015-12-18
 * **************************************************/

#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "jvm.h"
#include "runtime.h"

static int sFrameIdx = 0;
static int sOperandIdx = 0;

static void executeNative(ExecEnv *env, const MethodEntry *method) {
#ifdef LOG_DETAIL
    printf("\t<natvie %s:%s>\n", 
            method->name, method->type);
#endif

    NativeFuncPtr funcPtr = retrieveNativeMethod(method);
    if (funcPtr != NULL) {
        StackFrame *frame = peekJavaStack(env->javaStack);
        OperandStack *stack = frame->opdStack;

        if (stack->validCnt != 2) {
            assert(0 && "just support native method with 1 param now");
        }

        // TODO
        // util now just support one argument
        // const int cnt = stack->validCnt;

        Slot *slot = stack->slots;
        assert(slot);
        assert(slot->tag == ReferenceType);
        RefHandle *ref = (RefHandle *)slot->value;
        assert(ref->cls_ptr);
        assert(ref->obj_ptr);

        Slot *param = stack->slots + 1;
        assert(param);

        // TODO
        // util now just support one argument
        popOperandStack(stack);
        popOperandStack(stack);

        funcPtr(env, ref->obj_ptr, param);

    } else {
        ClassEntry *cls = CLASS_CE(method->class);
        printf("\t*Failed retrieve native method:%s.%s:%s\n", cls->name, method->name, method->type);

    }

    if (NULL != env->dl_handle) {
        dlclose(env->dl_handle);
    }

}

void executeMethod(ExecEnv *env, MethodEntry *method)
{
    assert(NULL != env && NULL != method);

    if (ACC_NATIVE & method->acc_flags) {
        executeNative(env, method);
        return;
    }

#ifdef LOG_DETAIL
    char* clsname = CLASS_CE(method->class)->name;
    printf("\t  [+*** %s.%s +***]\n", clsname, method->name);
#endif

    StackFrame *frame = obtainStackFrame();
    assert (NULL != frame);

    OperandStack *oprdStack = obtainSlotBuffer();
    assert (NULL != oprdStack);

#ifdef DEBUG
    frame->id = sFrameIdx++;
    oprdStack->id = sOperandIdx++;
#endif

    if (ensureSlotBufferCap(oprdStack, method->max_stack) < 0) {
        printf("Failed ensure operand statck capacity");
        exit(-1);
    }

    LocalVarTable *localTbl = obtainSlotBuffer();
    assert (NULL != localTbl);
    if (ensureSlotBufferCap(localTbl, method->max_locals) < 0) {
        printf("Failed ensure local variable table capacity");
        exit(-1);
    }

    frame->retAddr = env->reg_pc;
    frame->method = method;
    frame->localTbl = localTbl;
    frame->opdStack = oprdStack;
    frame->constPool = CLASS_CE(method->class)->constPool;

    if (!pushJavaStack(env->javaStack, frame)) {
        printf ("Failed push stack frame to java stack.\n");
        exit (1);
    }

    cond_signal(env->cond);

#ifdef LOG_DETAIL
    printf("\t  [+... stack frame:%d +...]\n", frame->id);
#endif


#if 0
    // extract & parse instructions from the byte code
    extractInstructions((MethodEntry *)method);

    InstExecEnv instEnv;
    const Instruction *inst = NULL;
    int i;
    for (i = 0;  i < method->instCnt; i++) {
        memset(&instEnv, 0, sizeof(instEnv));
        inst = method->instTbl[i];
        instEnv.inst = (Instruction *)inst;
        instEnv.env  = env;
        instEnv.method = method;
        instEnv.method_pos = i;

        inst->handler(&instEnv);
    }
#endif

#ifdef LOG_DETAIL
    printf("\t  [-*** %s.%s -***]\n", clsname, method->name);
#endif

}

/*  
 * pop operands from statck to local table
 */
static void popOpdStackToLocalTbl(OperandStack* stack, 
        LocalVarTable *tbl, int count) {

    assert (stack != NULL);
    assert (tbl != NULL);
    assert (count >= 0);
    assert (stack->validCnt >= count);
    assert (tbl->capacity >= count);

    Slot* s;
    int i;
    for (i = count - 1; i >= 0; i--) {
       s = popOperandStack(stack); 
       memcpy(tbl->slots + i, s, sizeof(*s));
    }
    tbl->validCnt = count;
}

/*
 *  Execute constructor, 
 *  the parameters will popped from OperandStack
 *  and placed in LocalTable, these will be done before enter INST_FUNCs
 */
void executeMethod_spec(ExecEnv *env, MethodEntry *method)
{
    assert(NULL != env && NULL != method);

    if (ACC_NATIVE & method->acc_flags) {
        executeNative(env, method);
        return;
    }

    // do not just support constructor only!
    // do not forget to refacor me !
#if 0
    assert (!strcmp(method->name, "<init>"));
#endif

#ifdef LOG_DETAIL
    char* clsname = CLASS_CE(method->class)->name;
    printf("\t  {+*** %s.%s +***}\n", clsname, method->name);
#endif

    StackFrame *frame = obtainStackFrame();
    assert (NULL != frame);

    OperandStack *oprdStack = obtainSlotBuffer();
    assert (NULL != oprdStack);

#ifdef DEBUG
    frame->id = sFrameIdx++;
    oprdStack->id = sOperandIdx++;
#endif

    if (ensureSlotBufferCap(oprdStack, method->max_stack) < 0) {
        printf("Failed ensure operand statck capacity");
        exit(-1);
    }

    LocalVarTable *localTbl = obtainSlotBuffer();
    assert (NULL != localTbl);
    if (ensureSlotBufferCap(localTbl, method->max_locals) < 0) {
        printf("Failed ensure local variable table capacity");
        exit(-1);
    }

    frame->retAddr = env->reg_pc;
    frame->method = method;
    frame->localTbl = localTbl;
    frame->opdStack = oprdStack;
    frame->constPool = CLASS_CE(method->class)->constPool;

    /* pop operands from statck to local table */
    StackFrame *top = peekJavaStack(env->javaStack);
    OperandStack *stack = top->opdStack;
    // usually, max_localal equals args_count except main(String[])
    popOpdStackToLocalTbl(stack, frame->localTbl, method->max_locals);
    /** pop finish **/

    if (!pushJavaStack(env->javaStack, frame)) {
        printf ("Failed push stack frame to java stack.\n");
        exit (1);
    }
    cond_signal(env->cond);

#ifdef LOG_DETAIL
    printf("\t  [+... stack frame:%d +...]\n", frame->id);
#endif

#ifdef LOG_DETAIL
    printf("\t  {-*** %s.%s -***}\n", clsname, method->name);
#endif

}

void* engineRoutine(void *param)
{
//#ifdef LOG_DETAIL
	printf("engineRoutine\n");
//#endif

    assert(param);
    ExecEnv *env = (ExecEnv *)param;
    JavaStack *stack = env->javaStack;
    while (!env->exitFlag) {
        while (isJavaStackEmpty(stack)) {
            cond_wait(env->cond, env->mutex);
        }

        StackFrame* frame = popJavaStack(stack);

//#ifdef LOG_DETAIL
        printf("engine routine runing...\n");
		printf("reg PC:%d\n", env->reg_pc);
//#endif

#if 0
        MethodEntry* method = peekJavaStack(env->javaStack)->method;
        // extract & parse instructions from the byte code
        extractInstructions((MethodEntry *)method);

        InstExecEnv instEnv;
        const Instruction *inst = NULL;
        for (; env->reg_pc < method->instCnt; env->reg_pc++) {
            printf("reg_pc:%d\n", env->reg_pc);
            memset(&instEnv, 0, sizeof(instEnv));
            inst = method->instTbl[env->reg_pc];
            instEnv.inst = (Instruction *)inst;
            instEnv.env  = env;
            instEnv.method = method;
            instEnv.method_pos = env->reg_pc;
            inst->handler(&instEnv);
        }
#endif

    }
    printf("exit engineRoutine .\n");
    return NULL;
}
