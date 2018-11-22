// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECTS_VISITING_INL_H_
#define V8_HEAP_OBJECTS_VISITING_INL_H_

#include "src/heap/objects-visiting.h"

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/mark-compact.h"
#include "src/macro-assembler.h"
#include "src/objects-body-descriptors-inl.h"
#include "src/objects-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

template <typename ResultType, typename ConcreteVisitor>
template <typename T, typename>
T* HeapVisitor<ResultType, ConcreteVisitor>::Cast(HeapObject* object) {
  return T::cast(object);
}

template <typename ResultType, typename ConcreteVisitor>
template <typename T, typename>
T HeapVisitor<ResultType, ConcreteVisitor>::Cast(HeapObject* object) {
  return T::cast(object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit(HeapObject* object) {
  return Visit(object->map(), object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit(Map map,
                                                           HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  switch (map->visitor_id()) {
#define CASE(TypeName, Type)         \
  case kVisit##TypeName:             \
    return visitor->Visit##TypeName( \
        map, ConcreteVisitor::template Cast<TypeName>(object));
    TYPED_VISITOR_ID_LIST(CASE)
#undef CASE
    case kVisitShortcutCandidate:
      return visitor->VisitShortcutCandidate(
          map, ConcreteVisitor::template Cast<ConsString>(object));
    case kVisitNativeContext:
      return visitor->VisitNativeContext(
          map, ConcreteVisitor::template Cast<NativeContext>(object));
    case kVisitDataObject:
      return visitor->VisitDataObject(map, object);
    case kVisitJSObjectFast:
      return visitor->VisitJSObjectFast(
          map, ConcreteVisitor::template Cast<JSObject>(object));
    case kVisitJSApiObject:
      return visitor->VisitJSApiObject(
          map, ConcreteVisitor::template Cast<JSObject>(object));
    case kVisitStruct:
      return visitor->VisitStruct(map, object);
    case kVisitFreeSpace:
      return visitor->VisitFreeSpace(map, FreeSpace::cast(object));
    case kVisitWeakArray:
      return visitor->VisitWeakArray(map, object);
    case kVisitJSWeakCell:
      return visitor->VisitJSWeakCell(map, JSWeakCell::cast(object));
    case kVisitorIdCount:
      UNREACHABLE();
  }
  UNREACHABLE();
  // Make the compiler happy.
  return ResultType();
}

template <typename ResultType, typename ConcreteVisitor>
void HeapVisitor<ResultType, ConcreteVisitor>::VisitMapPointer(HeapObject* host,
                                                               ObjectSlot map) {
  static_cast<ConcreteVisitor*>(this)->VisitPointer(host, map);
}

#define VISIT(TypeName, Type)                                                  \
  template <typename ResultType, typename ConcreteVisitor>                     \
  ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit##TypeName(        \
      Map map, Type object) {                                                  \
    ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);            \
    if (!visitor->ShouldVisit(object)) return ResultType();                    \
    if (!visitor->AllowDefaultJSObjectVisit()) {                               \
      DCHECK_WITH_MSG(!map->IsJSObjectMap(),                                   \
                      "Implement custom visitor for new JSObject subclass in " \
                      "concurrent marker");                                    \
    }                                                                          \
    int size = TypeName::BodyDescriptor::SizeOf(map, object);                  \
    if (visitor->ShouldVisitMapPointer())                                      \
      visitor->VisitMapPointer(object, object->map_slot());                    \
    TypeName::BodyDescriptor::IterateBody(map, object, size, visitor);         \
    return static_cast<ResultType>(size);                                      \
  }
TYPED_VISITOR_ID_LIST(VISIT)
#undef VISIT

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitShortcutCandidate(
    Map map, ConsString* object) {
  return static_cast<ConcreteVisitor*>(this)->VisitConsString(map, object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitNativeContext(
    Map map, NativeContext* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  int size = NativeContext::BodyDescriptor::SizeOf(map, object);
  NativeContext::BodyDescriptor::IterateBody(map, object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitDataObject(
    Map map, HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = map->instance_size();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSObjectFast(
    Map map, JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = JSObject::FastBodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  JSObject::FastBodyDescriptor::IterateBody(map, object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSApiObject(
    Map map, JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = JSObject::BodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  JSObject::BodyDescriptor::IterateBody(map, object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitStruct(
    Map map, HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = map->instance_size();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  StructBodyDescriptor::IterateBody(map, object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitFreeSpace(
    Map map, FreeSpace* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  return static_cast<ResultType>(FreeSpace::cast(object)->size());
}

template <typename ConcreteVisitor>
int NewSpaceVisitor<ConcreteVisitor>::VisitNativeContext(
    Map map, NativeContext* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = NativeContext::BodyDescriptor::SizeOf(map, object);
  NativeContext::BodyDescriptor::IterateBody(map, object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
int NewSpaceVisitor<ConcreteVisitor>::VisitJSApiObject(Map map,
                                                       JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  return visitor->VisitJSObject(map, object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitWeakArray(
    Map map, HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = WeakArrayBodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  WeakArrayBodyDescriptor::IterateBody(map, object, size, visitor);
  return size;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECTS_VISITING_INL_H_
