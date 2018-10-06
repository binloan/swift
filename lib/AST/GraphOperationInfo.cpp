//===--- GraphOperationInfo.cpp - GraphOperationInst Parse Logic ----------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/SIL/GraphOperationInfo.h"
#include "swift/SIL/PrettyStackTrace.h"
#include "swift/SIL/SILConstants.h"
#include "swift/SIL/SILInstruction.h"

using llvm::SmallVectorImpl;
using llvm::StringRef;
using namespace swift;
using namespace tf;

GraphOperationInfo::GraphOperationInfo(GraphOperationInst *inst) : inst(inst) {
  PrettyStackTraceSILNode X("decoding graph_op name", inst);

  ArrayRef<Operand> remainingOperands = inst->getAllOperands();
  StringRef remainingMangled = inst->getName().str();
  auto nextMarkerPos = remainingMangled.find(',');
  OperationName = remainingMangled.substr(0, nextMarkerPos);

  while (nextMarkerPos != StringRef::npos) {
    remainingMangled = remainingMangled.drop_front(nextMarkerPos);
    nextMarkerPos = remainingMangled.find(',', 1);

    StringRef thisMarker = remainingMangled.substr(0, nextMarkerPos);
    StringRef thisMarkerName = thisMarker.drop_front(2);
    assert(thisMarker.size() >= 2 && "marker too short");
    switch (thisMarker[1]) {
    case 'i':
      // Push a SAK_Single.
      StructuredArguments.emplace_back(SAK_Single, thisMarkerName,
                                      remainingOperands.front().get());
      remainingOperands = remainingOperands.drop_front(1);
      break;
    case 'L':
      // Push a SAK_List with ArgumentList of size 0 pointing at the right place
      // in the inst's arguments.
      StructuredArguments.emplace_back(SAK_List, thisMarkerName,
                                      remainingOperands.take_front(0));
      break;
    case 'e':
      // Extend the ArgumentList of the curent SAK_List by 1 to include the next
      // of the inst's arguments.
      assert(StructuredArguments.size() > 0 && "list element not in list");
      assert(StructuredArguments.back().Kind == SAK_List &&
             "list element not in list");
      assert(thisMarkerName.empty() && "list element should not have name");
      StructuredArguments.back().ArgumentList = ArrayRef<Operand>(
          StructuredArguments.back().ArgumentList.data(),
          StructuredArguments.back().ArgumentList.size() + 1);
      remainingOperands = remainingOperands.drop_front(1);
      break;
    default:
      llvm_unreachable("unknown marker kind");
    }
  }
}

int64_t GraphOperationInfo::getIntAttr(unsigned attrIdx,
                                       StringRef attrName) const {
  auto attr = inst->getAttribute(attrIdx);
  auto attrInfo = GraphOperationInfo::decodeArgumentName(attr.name.str());
  assert(attrInfo.first == attrName);
  auto attrValue = attr.value;
  return attrValue.getIntegerValue().getLimitedValue();
}

std::string GraphOperationInfo::getStringAttr(unsigned attrIdx,
                                              StringRef attrName) const {
  auto attr = inst->getAttribute(attrIdx);
  auto attrInfo = GraphOperationInfo::decodeArgumentName(attr.name.str());
  assert(attrInfo.first == attrName);
  auto attrValue = attr.value;
  return attrValue.getStringValue().str();
}

void GraphOperationInfo::assertWithDump(bool cond,
                                        const char *assertMsg) const {
#ifndef NDEBUG
  if (cond)
    return;
  inst->dump();
  llvm_unreachable(assertMsg);
#endif // NDEBUG
}

/// Return the string suffix for the specified ArgumentLowering.
const char *
GraphOperationInfo::getArgumentLoweringSuffix(ArgumentLowering lowering) {
  switch (lowering) {
  case ArgumentLowering::Input:
    return "";
  case ArgumentLowering::NormalAttribute:
    return "";
  case ArgumentLowering::TensorAttribute:
    return "$tensor";
  case ArgumentLowering::ShapeAttribute:
    return "$shape";
  case ArgumentLowering::UnknownShapeListAttribute:
    return "$unknownShapeList";
  case ArgumentLowering::TypeListAttribute:
    return "$typeList";
  case ArgumentLowering::Out:
    return "$out";
  }
}

/// Given an argument name like foo$tensor, decode the name and the
/// ArgumentLowering.  If the name is empty, this defaults to
/// ArgumentLowering::Input.  If the name is non-empty but there is no
/// modifier specified, then this defaults to
/// ArgumentLowering::NormalAttribute.
std::pair<StringRef, GraphOperationInfo::ArgumentLowering>
GraphOperationInfo::decodeArgumentName(StringRef Name) {
  if (Name.empty())
    return {Name, ArgumentLowering::Input};

  auto dollarLoc = Name.find('$');
  auto lowering = ArgumentLowering::NormalAttribute;
  if (dollarLoc != StringRef::npos) {
    auto suffix = Name.drop_front(dollarLoc + 1);
    auto loweringOpt =
        llvm::StringSwitch<llvm::Optional<ArgumentLowering>>(suffix)
          .Case("", ArgumentLowering::NormalAttribute)
          .Case("tensor", ArgumentLowering::TensorAttribute)
          .Case("shape", ArgumentLowering::ShapeAttribute)
          .Case("unknownShapeList", ArgumentLowering::UnknownShapeListAttribute)
          .Case("typeList", ArgumentLowering::TypeListAttribute)
          .Case("out", ArgumentLowering::Out)
          .Default(None);
    assert(loweringOpt && "invalid attribute modifier");
    lowering = *loweringOpt;
  }
  return {Name.substr(0, dollarLoc), lowering};
}

/// Returns this argument's name, without suffix, and the ArgumentLowering.
std::pair<StringRef, GraphOperationInfo::ArgumentLowering>
GraphOperationInfo::StructuredArgument::getArgumentNameAndLowering() const {
  return decodeArgumentName(Name);
}

/// Determine whether the specified type is one of our well-known types, and
/// if so, which one it is.
TFValueKind tf::classifyTensorFlowValue(SILType ty) {
  return classifyTensorFlowValue(ty.getASTType());
}
