#include "clox_stdio.h"
#include "clox_gfx.h"
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

VM vm; // [one]
static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}
void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", // [minus]
            function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}
GFX_DEFINE(millis, 0)
GFX_DEFINE(micros, 0)
GFX_DEFINE(delay, 1)
GFX_DEFINE(delayMicroseconds, 1)
GFX_DEFINE_NUM_STR(pinMode)
GFX_DEFINE_NUM_BOOL(digitalWrite)
GFX_DEFINE(digitalRead, 1)
GFX_DEFINE(analogWriteResolution, 1)
GFX_DEFINE(analogWrite, 2)
GFX_DEFINE(analogReadResolution, 1)
GFX_DEFINE(analogRead, 1)
GFX_DEFINE(analogReference, 1)
GFX_DEFINE(bit, 1)
GFX_DEFINE(bitClear, 2)
GFX_DEFINE(bitRead, 2)
GFX_DEFINE(bitSet, 2)
GFX_DEFINE(highByte, 1)
GFX_DEFINE(lowByte, 1)
GFX_DEFINE(isRotated, 0)
GFX_DEFINE(beginDraw, 0)
GFX_DEFINE(endDraw, 0)
GFX_DEFINE(width, 0)
GFX_DEFINE(height, 0)
GFX_DEFINE(fill, 3)
GFX_DEFINE(noFill, 0)
GFX_DEFINE(stroke, 3)
GFX_DEFINE(noStroke, 0)
GFX_DEFINE(background, 3)
GFX_DEFINE(clear, 0)
GFX_DEFINE(circle, 3)
GFX_DEFINE(ellipse, 4)
GFX_DEFINE(line, 4)
GFX_DEFINE(point, 2)
GFX_DEFINE(rect, 4)
GFX_DEFINE_STR_NUM_NUM(text)
GFX_DEFINE_STR(textFont)
GFX_DEFINE(textFontWidth, 0)
GFX_DEFINE(textFontHeight, 0)
GFX_DEFINE(beginText, 5)
GFX_DEFINE_STR(endText)
GFX_DEFINE(textScrollSpeed, 1)
GFX_DEFINE_STR(printStr)
GFX_DEFINE_STR(printStrLn)
GFX_DEFINE(printInt, 2)
GFX_DEFINE(printFloat, 2)

static Value appendNative(int argCount, Value* args) {
  // Append a value to the end of a list increasing the list's length by 1
  if (argCount != 2 || !IS_LIST(args[0])) {
    runtimeError("Bad call to append().");
    return ERR_VAL;
  }
  ObjList* list = AS_LIST(args[0]);
  Value item = args[1];
  appendToList(list, item);
  return NIL_VAL;
}

static Value deleteNative(int argCount, Value* args) {
  // Delete an item from a list at the given index.
  if (argCount != 2 || !IS_LIST(args[0]) || !IS_NUMBER(args[1])) {
    runtimeError("Bad call to delete().");
    return ERR_VAL;
  }

  ObjList* list = AS_LIST(args[0]);
  int index = AS_NUMBER(args[1]);

  if (!isValidListIndex(list, index)) {
    runtimeError("Index %d is not valid.", index);
    return ERR_VAL;
  }

  deleteFromList(list, index);
  return NIL_VAL;
}
static Value lengthNative(int argCount, Value* args) {
  if (argCount != 1 || (!IS_STRING(args[0]) && !IS_LIST(args[0]))) {
    runtimeError("Bad call to length().");
    return ERR_VAL;
  }
  int length = 0;
  if (IS_STRING(args[0])) {
    ObjString* str = AS_STRING(args[0]);
    length = str->length;
  }
  else if (IS_LIST(args[0])) {
    ObjList* list = AS_LIST(args[0]);
    length = list->count;
  }
  return NUMBER_VAL(length);
}
static Value tostringNative(int argCount, Value* args) {
  if (argCount != 1) {
    runtimeError("Bad call to tostring().");
    return ERR_VAL;
  }
  char buffer[16];
#ifdef NAN_BOXING
  if (IS_BOOL(args[0])) {
    snprintf(buffer, sizeof(buffer), AS_BOOL(args[0]) ? "true" : "false");
  } else if (IS_NIL(args[0])) {
    snprintf(buffer, sizeof(buffer), "nil");
  } else if (IS_NUMBER(args[0])) {
    snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(args[0]));
  } else if (IS_OBJ(args[0]) && IS_STRING(args[0])) {
    return args[0];
  } else {
    runtimeError("Cannot convert this type to a string.");
    return ERR_VAL;
  }
#else
  switch (args[0].type) {
    case VAL_BOOL:
      snprintf(buffer, sizeof(buffer), AS_BOOL(args[0]) ? "true" : "false");
      break;
    case VAL_NIL: snprintf(buffer, sizeof(buffer), "nil"); break;
    case VAL_NUMBER: snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(args[0])); break;
    case VAL_OBJ: if (IS_STRING(args[0])) { return args[0]; } else {
      runtimeError("Cannot convert this type to a string.");
    return ERR_VAL;
     }
  }
#endif
  return OBJ_VAL(copyString(buffer, (int)strlen(buffer)));
}
static Value substringNative(int argCount, Value* args) {
  if (argCount != 3 || !IS_STRING(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
    runtimeError("Bad call to substring().");
    return ERR_VAL;
  }
  ObjString* str = AS_STRING(args[0]);
  int start = AS_NUMBER(args[1]), end = AS_NUMBER(args[2]);
  if (start < 0 || start > end || end >= str->length) {
    runtimeError("Bad index(es) for substring().");
    return ERR_VAL;
  }
  return OBJ_VAL(copyString(str->chars + start, end - start + 1));
}
static void defineNative(const char* name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  initTable(&vm.globals);
  initTable(&vm.strings);

  vm.initString = NULL;
  vm.initString = copyString("init", 4);

  GFX_DECLARE(millis);
  GFX_DECLARE(micros);
  GFX_DECLARE(delay);
  GFX_DECLARE(delayMicroseconds);
  GFX_DECLARE(pinMode);
  GFX_DECLARE(digitalWrite);
  GFX_DECLARE(digitalRead);
  GFX_DECLARE(analogWriteResolution);
  GFX_DECLARE(analogWrite);
  GFX_DECLARE(analogReadResolution);
  GFX_DECLARE(analogRead);
  GFX_DECLARE(analogReference);
  GFX_DECLARE(beginDraw);
  GFX_DECLARE(bit);
  GFX_DECLARE(bitClear);
  GFX_DECLARE(bitRead);
  GFX_DECLARE(bitSet);
  GFX_DECLARE(highByte);
  GFX_DECLARE(lowByte);
  GFX_DECLARE(isRotated);
  GFX_DECLARE(endDraw);
  GFX_DECLARE(width);
  GFX_DECLARE(height);
  GFX_DECLARE(fill);
  GFX_DECLARE(noFill);
  GFX_DECLARE(stroke);
  GFX_DECLARE(noStroke);
  GFX_DECLARE(background);
  GFX_DECLARE(clear);
  GFX_DECLARE(circle);
  GFX_DECLARE(ellipse);
  GFX_DECLARE(line);
  GFX_DECLARE(point);
  GFX_DECLARE(rect);
  GFX_DECLARE(text);
  GFX_DECLARE(textFont);
  GFX_DECLARE(textFontWidth);
  GFX_DECLARE(textFontHeight);
  GFX_DECLARE(beginText);
  GFX_DECLARE(endText);
  GFX_DECLARE(textScrollSpeed);
  GFX_DECLARE(printStr);
  GFX_DECLARE(printStrLn);
  GFX_DECLARE(printInt);
  GFX_DECLARE(printFloat);
  defineNative("append", appendNative);
  defineNative("delete", deleteNative);
  defineNative("length", lengthNative);
  defineNative("tostring", tostringNative);
  defineNative("substring", substringNative);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjects();
}
void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}
Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}
static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}
static bool call(ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        closure->function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}
static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
        Value initializer;
        if (tableGet(&klass->methods, vm.initString,
                     &initializer)) {
          return call(AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d.",
                       argCount);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        if (result == ERR_VAL) {
          return false;
        }
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}
static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }
  return call(AS_CLOSURE(method), argCount);
}
static bool invoke(ObjString* name, int argCount) {
  Value receiver = peek(argCount);

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(receiver);

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

  return invokeFromClass(instance->klass, name, argCount);
}
static bool bindMethod(ObjClass* klass, ObjString* name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(peek(0),
                                         AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}
static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}
static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}
static void defineMethod(ObjString* name) {
  Value method = peek(0);
  ObjClass* klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}
static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}
static void concatenate() {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}
static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name); // [delete]
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_BUILD_LIST: {
        // Stack before: [item1, item2, ..., itemN] and after: [list]
        ObjList* list = newList();
        uint8_t itemCount = READ_BYTE();

        // Add items to list
        push(OBJ_VAL(list)); // So list isn't sweeped by GC in appendToList
        for (int i = itemCount; i > 0; i--) {
          appendToList(list, peek(i));
        }
        pop();

        // Pop items from stack
        while (itemCount-- > 0) {
          pop();
        }

        push(OBJ_VAL(list));
        break;
      }
      case OP_INDEX_SUBSCR: {
        // Stack before: [list, index] and after: [index(list, index)]
        Value index = pop();
        Value list = pop();
        Value result;

        if (!IS_LIST(list)) {
          runtimeError("Invalid type to index into.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjList* list_obj = AS_LIST(list);

        if (!IS_NUMBER(index)) {
          runtimeError("List index is not a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        int index_obj = AS_NUMBER(index);

        if (!isValidListIndex(list_obj, index_obj)) {
          runtimeError("List index out of range.");
          return INTERPRET_RUNTIME_ERROR;
        }

        result = indexFromList(list_obj, index_obj);
        push(result);
        break;
      }
      case OP_STORE_SUBSCR: {
        // Stack before: [list, index, item] and after: [item]
        Value item = pop();
        Value index = pop();
        Value list = pop();

        if (!IS_LIST(list)) {
          runtimeError("Cannot store value in a non-list.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjList* list_obj = AS_LIST(list);

        if (!IS_NUMBER(index)) {
          runtimeError("List index is not a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        int index_obj = AS_NUMBER(index);

        if (!isValidListIndex(list_obj, index_obj)) {
          runtimeError("Invalid list index.");
          return INTERPRET_RUNTIME_ERROR;
        }

        storeToList(list_obj, index_obj, item);
        push(item);
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjString* name = READ_STRING();
        
        Value value;
        if (tableGet(&instance->fields, name, &value)) {
          pop(); // Instance.
          push(value);
          break;
        }

        if (!bindMethod(instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(1));
        tableSet(&instance->fields, READ_STRING(), peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_GET_SUPER: {
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop());
        
        if (!bindMethod(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else if (IS_LIST(peek(0)) && IS_LIST(peek(1))) {
          ObjList* b = AS_LIST(pop());
          ObjList* a = AS_LIST(pop());
          for (int i = 0; i != b->count; ++i) {
            appendToList(a, b->items[i]);
          }
          push(OBJ_VAL(a));
        } else {
          runtimeError(
              "Operands must be two numbers two lists or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] =
                captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STRING())));
        break;
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeError("Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjClass* subclass = AS_CLASS(peek(0));
        tableAddAll(&AS_CLASS(superclass)->methods,
                    &subclass->methods);
        pop(); // Subclass.
        break;
      }
      case OP_METHOD:
        defineMethod(READ_STRING());
        break;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}
InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}
