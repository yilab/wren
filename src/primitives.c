#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "primitives.h"
#include "value.h"

#define PRIMITIVE(cls, name, prim) \
    { \
      int symbol = ensureSymbol(&vm->methods, name, strlen(name)); \
      cls->methods[symbol].type = METHOD_PRIMITIVE; \
      cls->methods[symbol].primitive = primitive_##prim; \
    }

#define FIBER_PRIMITIVE(cls, name, prim) \
    { \
      int symbol = ensureSymbol(&vm->methods, name, strlen(name)); \
      cls->methods[symbol].type = METHOD_FIBER; \
      cls->methods[symbol].fiberPrimitive = primitive_##prim; \
    }

#define DEF_PRIMITIVE(prim) \
    static Value primitive_##prim(WrenVM* vm, Value* args)

#define DEF_FIBER_PRIMITIVE(prim) \
    static void primitive_##prim(WrenVM* vm, Fiber* fiber, Value* args)

// TODO(bob): Tune these.
#define LIST_MIN_CAPACITY (16)
#define LIST_GROW_FACTOR (2)

DEF_PRIMITIVE(bool_not)
{
  return BOOL_VAL(!AS_BOOL(args[0]));
}

DEF_PRIMITIVE(bool_toString)
{
  // TODO(bob): Intern these strings or something.
  if (AS_BOOL(args[0]))
  {
    return wrenNewString(vm, "true", 4);
  }
  else
  {
    return wrenNewString(vm, "false", 5);
  }
}

// The call instruction leading to this primitive has one argument for the
// receiver plus as many arguments as were passed. When we push the block onto
// the callstack, we again use as many arguments. That ensures that the result
// of evaluating the block goes into the slot that the caller of *this*
// primitive is expecting.
DEF_FIBER_PRIMITIVE(fn_call0) { callFunction(fiber, AS_FN(args[0]), 1); }
DEF_FIBER_PRIMITIVE(fn_call1) { callFunction(fiber, AS_FN(args[0]), 2); }
DEF_FIBER_PRIMITIVE(fn_call2) { callFunction(fiber, AS_FN(args[0]), 3); }
DEF_FIBER_PRIMITIVE(fn_call3) { callFunction(fiber, AS_FN(args[0]), 4); }
DEF_FIBER_PRIMITIVE(fn_call4) { callFunction(fiber, AS_FN(args[0]), 5); }
DEF_FIBER_PRIMITIVE(fn_call5) { callFunction(fiber, AS_FN(args[0]), 6); }
DEF_FIBER_PRIMITIVE(fn_call6) { callFunction(fiber, AS_FN(args[0]), 7); }
DEF_FIBER_PRIMITIVE(fn_call7) { callFunction(fiber, AS_FN(args[0]), 8); }
DEF_FIBER_PRIMITIVE(fn_call8) { callFunction(fiber, AS_FN(args[0]), 9); }

// Grows [list] if needed to ensure it can hold [count] elements.
static void ensureListCapacity(WrenVM* vm, ObjList* list, int count)
{
  if (list->capacity >= count) return;

  int capacity = list->capacity * LIST_GROW_FACTOR;
  if (capacity < LIST_MIN_CAPACITY) capacity = LIST_MIN_CAPACITY;

  list->capacity *= 2;
  list->elements = wrenReallocate(vm, list->elements,
      list->capacity * sizeof(Value), capacity * sizeof(Value));
  // TODO(bob): Handle allocation failure.
  list->capacity = capacity;
}

// Validates that [index] is an integer within `[0, count)`. Also allows
// negative indices which map backwards from the end. Returns the valid positive
// index value, or -1 if the index wasn't valid (not a number, not an int, out
// of bounds).
static int validateIndex(Value index, int count)
{
  if (!IS_NUM(index)) return -1;

  double indexNum = AS_NUM(index);
  int intIndex = (int)indexNum;
  // Make sure the index is an integer.
  if (indexNum != intIndex) return -1;

  // Negative indices count from the end.
  if (indexNum < 0) indexNum = count + indexNum;

  // Check bounds.
  if (indexNum < 0 || indexNum >= count) return -1;
  
  return indexNum;
}

DEF_PRIMITIVE(list_add)
{
  ObjList* list = AS_LIST(args[0]);

  ensureListCapacity(vm, list, list->count + 1);
  list->elements[list->count++] = args[1];
  return args[1];
}

DEF_PRIMITIVE(list_clear)
{
  ObjList* list = AS_LIST(args[0]);
  wrenReallocate(vm, list->elements, list->capacity * sizeof(Value), 0);
  list->capacity = 0;
  list->count = 0;
  return NULL_VAL;
}

DEF_PRIMITIVE(list_count)
{
  ObjList* list = AS_LIST(args[0]);
  return NUM_VAL(list->count);
}

DEF_PRIMITIVE(list_insert)
{
  ObjList* list = AS_LIST(args[0]);

  // count + 1 here so you can "insert" at the very end.
  int index = validateIndex(args[2], list->count + 1);
  // TODO(bob): Instead of returning null here, should signal an error
  // explicitly somehow.
  if (index == -1) return NULL_VAL;

  ensureListCapacity(vm, list, list->count + 1);

  // Shift items down.
  for (int i = list->count; i > index; i--)
  {
    list->elements[i] = list->elements[i - 1];
  }

  list->elements[index] = args[1];
  list->count++;
  return args[1];
}

DEF_PRIMITIVE(list_removeAt)
{
  ObjList* list = AS_LIST(args[0]);
  int index = validateIndex(args[1], list->count);
  // TODO(bob): Instead of returning null here, should signal an error
  // explicitly somehow.
  if (index == -1) return NULL_VAL;

  Value removed = list->elements[index];

  // Shift items up.
  for (int i = index; i < list->count - 1; i++)
  {
    list->elements[i] = list->elements[i + 1];
  }

  // If we have too much excess capacity, shrink it.
  if (list->capacity / LIST_GROW_FACTOR >= list->count)
  {
    wrenReallocate(vm, list->elements, sizeof(Value) * list->capacity,
                   sizeof(Value) * (list->capacity / LIST_GROW_FACTOR));
    list->capacity /= LIST_GROW_FACTOR;
  }

  list->count--;
  return removed;
}

DEF_PRIMITIVE(list_subscript)
{
  ObjList* list = AS_LIST(args[0]);

  int index = validateIndex(args[1], list->count);
  // TODO(bob): Instead of returning null here, should signal an error
  // explicitly somehow.
  if (index == -1) return NULL_VAL;

  return list->elements[index];
}

DEF_PRIMITIVE(num_abs)
{
  return NUM_VAL(fabs(AS_NUM(args[0])));
}

DEF_PRIMITIVE(num_toString)
{
  // TODO(bob): What size should this be?
  char temp[100];
  sprintf(temp, "%.14g", AS_NUM(args[0]));
  return (Value)wrenNewString(vm, temp, strlen(temp));
}

DEF_PRIMITIVE(num_negate)
{
  return NUM_VAL(-AS_NUM(args[0]));
}

DEF_PRIMITIVE(num_minus)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return NUM_VAL(AS_NUM(args[0]) - AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_plus)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  // TODO(bob): Handle coercion to string if RHS is a string.
  return NUM_VAL(AS_NUM(args[0]) + AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_multiply)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return NUM_VAL(AS_NUM(args[0]) * AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_divide)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return NUM_VAL(AS_NUM(args[0]) / AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_mod)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return NUM_VAL(fmod(AS_NUM(args[0]), AS_NUM(args[1])));
}

DEF_PRIMITIVE(num_lt)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return BOOL_VAL(AS_NUM(args[0]) < AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_gt)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return BOOL_VAL(AS_NUM(args[0]) > AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_lte)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return BOOL_VAL(AS_NUM(args[0]) <= AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_gte)
{
  if (!IS_NUM(args[1])) return vm->unsupported;
  return BOOL_VAL(AS_NUM(args[0]) >= AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_eqeq)
{
  if (!IS_NUM(args[1])) return FALSE_VAL;
  return BOOL_VAL(AS_NUM(args[0]) == AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_bangeq)
{
  if (!IS_NUM(args[1])) return TRUE_VAL;
  return BOOL_VAL(AS_NUM(args[0]) != AS_NUM(args[1]));
}

DEF_PRIMITIVE(object_eqeq)
{
  return BOOL_VAL(wrenValuesEqual(args[0], args[1]));
}

DEF_PRIMITIVE(object_bangeq)
{
  return BOOL_VAL(!wrenValuesEqual(args[0], args[1]));
}

DEF_PRIMITIVE(object_type)
{
  return OBJ_VAL(wrenGetClass(vm, args[0]));
}

DEF_PRIMITIVE(string_contains)
{
  const char* string = AS_CSTRING(args[0]);
  // TODO(bob): Check type of arg first!
  const char* search = AS_CSTRING(args[1]);

  // Corner case, the empty string contains the empty string.
  if (strlen(string) == 0 && strlen(search) == 0) return TRUE_VAL;

  return BOOL_VAL(strstr(string, search) != NULL);
}

DEF_PRIMITIVE(string_count)
{
  double count = strlen(AS_CSTRING(args[0]));
  return NUM_VAL(count);
}

DEF_PRIMITIVE(string_toString)
{
  return args[0];
}

DEF_PRIMITIVE(string_plus)
{
  if (!IS_STRING(args[1])) return vm->unsupported;
  // TODO(bob): Handle coercion to string of RHS.

  const char* left = AS_CSTRING(args[0]);
  const char* right = AS_CSTRING(args[1]);

  size_t leftLength = strlen(left);
  size_t rightLength = strlen(right);

  Value value = wrenNewString(vm, NULL, leftLength + rightLength);
  ObjString* string = AS_STRING(value);
  strcpy(string->value, left);
  strcpy(string->value + leftLength, right);
  string->value[leftLength + rightLength] = '\0';

  return value;
}

DEF_PRIMITIVE(string_eqeq)
{
  if (!IS_STRING(args[1])) return FALSE_VAL;
  const char* a = AS_CSTRING(args[0]);
  const char* b = AS_CSTRING(args[1]);
  return BOOL_VAL(strcmp(a, b) == 0);
}

DEF_PRIMITIVE(string_bangeq)
{
  if (!IS_STRING(args[1])) return TRUE_VAL;
  const char* a = AS_CSTRING(args[0]);
  const char* b = AS_CSTRING(args[1]);
  return BOOL_VAL(strcmp(a, b) != 0);
}

DEF_PRIMITIVE(string_subscript)
{
  // TODO(bob): Instead of returning null here, all of these failure cases
  // should signal an error explicitly somehow.
  if (!IS_NUM(args[1])) return NULL_VAL;

  double indexNum = AS_NUM(args[1]);
  int index = (int)indexNum;
  // Make sure the index is an integer.
  if (indexNum != index) return NULL_VAL;

  ObjString* string = AS_STRING(args[0]);

  // Negative indices count from the end.
  // TODO(bob): Strings should cache their length.
  int length = (int)strlen(string->value);
  if (index < 0) index = length + index;

  // Check bounds.
  if (index < 0 || index >= length) return NULL_VAL;

  // The result is a one-character string.
  // TODO(bob): Handle UTF-8.
  Value value = wrenNewString(vm, NULL, 2);
  ObjString* result = AS_STRING(value);
  result->value[0] = AS_CSTRING(args[0])[index];
  result->value[1] = '\0';
  return value;
}

DEF_PRIMITIVE(io_write)
{
  wrenPrintValue(args[1]);
  printf("\n");
  return args[1];
}

DEF_PRIMITIVE(os_clock)
{
  double time = (double)clock() / CLOCKS_PER_SEC;
  return NUM_VAL(time);
}

static ObjClass* defineClass(WrenVM* vm, const char* name, ObjClass* superclass)
{
  ObjClass* classObj = wrenNewClass(vm, superclass, 0);
  int symbol = addSymbol(&vm->globalSymbols, name, strlen(name));
  vm->globals[symbol] = OBJ_VAL(classObj);
  return classObj;
}

void wrenLoadCore(WrenVM* vm)
{
  vm->objectClass = defineClass(vm, "Object", NULL);
  PRIMITIVE(vm->objectClass, "== ", object_eqeq);
  PRIMITIVE(vm->objectClass, "!= ", object_bangeq);
  PRIMITIVE(vm->objectClass, "type", object_type);

  // The "Class" class is the superclass of all metaclasses.
  vm->classClass = defineClass(vm, "Class", vm->objectClass);

  vm->boolClass = defineClass(vm, "Bool", vm->objectClass);
  PRIMITIVE(vm->boolClass, "toString", bool_toString);
  PRIMITIVE(vm->boolClass, "!", bool_not);

  vm->fnClass = defineClass(vm, "Function", vm->objectClass);
  FIBER_PRIMITIVE(vm->fnClass, "call", fn_call0);
  FIBER_PRIMITIVE(vm->fnClass, "call ", fn_call1);
  FIBER_PRIMITIVE(vm->fnClass, "call  ", fn_call2);
  FIBER_PRIMITIVE(vm->fnClass, "call   ", fn_call3);
  FIBER_PRIMITIVE(vm->fnClass, "call    ", fn_call4);
  FIBER_PRIMITIVE(vm->fnClass, "call     ", fn_call5);
  FIBER_PRIMITIVE(vm->fnClass, "call      ", fn_call6);
  FIBER_PRIMITIVE(vm->fnClass, "call       ", fn_call7);
  FIBER_PRIMITIVE(vm->fnClass, "call        ", fn_call8);

  vm->listClass = defineClass(vm, "List", vm->objectClass);
  PRIMITIVE(vm->listClass, "add ", list_add);
  PRIMITIVE(vm->listClass, "clear", list_clear);
  PRIMITIVE(vm->listClass, "count", list_count);
  PRIMITIVE(vm->listClass, "insert  ", list_insert);
  PRIMITIVE(vm->listClass, "removeAt ", list_removeAt);
  PRIMITIVE(vm->listClass, "[ ]", list_subscript);

  vm->nullClass = defineClass(vm, "Null", vm->objectClass);

  vm->numClass = defineClass(vm, "Num", vm->objectClass);
  PRIMITIVE(vm->numClass, "abs", num_abs);
  PRIMITIVE(vm->numClass, "toString", num_toString)
  PRIMITIVE(vm->numClass, "-", num_negate);
  PRIMITIVE(vm->numClass, "- ", num_minus);
  PRIMITIVE(vm->numClass, "+ ", num_plus);
  PRIMITIVE(vm->numClass, "* ", num_multiply);
  PRIMITIVE(vm->numClass, "/ ", num_divide);
  PRIMITIVE(vm->numClass, "% ", num_mod);
  PRIMITIVE(vm->numClass, "< ", num_lt);
  PRIMITIVE(vm->numClass, "> ", num_gt);
  PRIMITIVE(vm->numClass, "<= ", num_lte);
  PRIMITIVE(vm->numClass, ">= ", num_gte);
  // TODO(bob): The only reason there are here is so that 0 != -0. Is that what
  // we want?
  PRIMITIVE(vm->numClass, "== ", num_eqeq);
  PRIMITIVE(vm->numClass, "!= ", num_bangeq);

  vm->stringClass = defineClass(vm, "String", vm->objectClass);
  PRIMITIVE(vm->stringClass, "contains ", string_contains);
  PRIMITIVE(vm->stringClass, "count", string_count);
  PRIMITIVE(vm->stringClass, "toString", string_toString)
  PRIMITIVE(vm->stringClass, "+ ", string_plus);
  PRIMITIVE(vm->stringClass, "== ", string_eqeq);
  PRIMITIVE(vm->stringClass, "!= ", string_bangeq);
  PRIMITIVE(vm->stringClass, "[ ]", string_subscript);

  ObjClass* ioClass = defineClass(vm, "IO", vm->objectClass);
  PRIMITIVE(ioClass, "write ", io_write);

  // TODO(bob): Making this an instance is lame. The only reason we're doing it
  // is because "IO.write()" looks ugly. Maybe just get used to that?
  Value ioObject = wrenNewInstance(vm, ioClass);
  vm->globals[addSymbol(&vm->globalSymbols, "io", 2)] = ioObject;

  ObjClass* osClass = defineClass(vm, "OS", vm->objectClass);
  PRIMITIVE(osClass->metaclass, "clock", os_clock);

  // TODO(bob): Make this a distinct object type.
  ObjClass* unsupportedClass = wrenNewClass(vm, vm->objectClass, 0);
  vm->unsupported = (Value)wrenNewInstance(vm, unsupportedClass);
}
