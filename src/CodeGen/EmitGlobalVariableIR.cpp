// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "EmitGlobalVariableIR.h"

#include "Base/ExprDispatcher/ExprDispatcher.h"
#include "CGModule.h"
#include "DIBuilder.h"
#include "EmitExpressionIR.h"
#include "IRBuilder.h"
#include "IRGenerator.h"
#include "cangjie/CHIR/Value.h"

namespace Cangjie {
namespace CodeGen {
class GlobalVariableGeneratorImpl : public IRGeneratorImpl {
public:
    GlobalVariableGeneratorImpl(CGModule& cgMod, const std::vector<CHIR::GlobalVar*>& chirGVs)
        : cgMod(cgMod), chirGVs(chirGVs)
    {
    }

    void EmitIR() override;

private:
    CGModule& cgMod;
    const std::vector<CHIR::GlobalVar*> chirGVs;
};

template <> class IRGenerator<GlobalVariableGeneratorImpl> : public IRGenerator<> {
public:
    IRGenerator(CGModule& cgMod, const std::vector<CHIR::GlobalVar*>& chirGVs)
        : IRGenerator<>(std::make_unique<GlobalVariableGeneratorImpl>(cgMod, chirGVs))
    {
    }
};

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
void GlobalVariableGeneratorImpl::EmitIR()
{
    IRBuilder2 irBuilder(cgMod);
    std::set<CHIR::GlobalVar*> quickGVs(chirGVs.begin(), chirGVs.end());
    for (auto chirGV : cgMod.GetCGContext().GetCHIRPackage().GetGlobalVars()) {
        auto rawGV = llvm::cast<llvm::GlobalVariable>(cgMod.GetOrInsertGlobalVariable(chirGV)->GetRawValue());
        cgMod.diBuilder->CreateGlobalVar(*chirGV);
        if (quickGVs.find(chirGV) == quickGVs.end()) {
            continue;
        }
        const auto align = cgMod.GetLLVMModule()->getDataLayout().getPrefTypeAlignment(rawGV->getType());
        rawGV->setAlignment(llvm::MaybeAlign(align));
        if (auto literalValue = chirGV->GetInitializer()) {
            rawGV->setInitializer(llvm::cast<llvm::Constant>(HandleLiteralValue(irBuilder, *literalValue)));
            if (chirGV->TestAttr(CHIR::Attribute::READONLY)) {
                rawGV->addAttribute(llvm::Attribute::ReadOnly);
                rawGV->setConstant(true);
            }
        } else {
            auto chirType = StaticCast<CHIR::RefType*>(chirGV->GetType())->GetBaseType();
            rawGV->setInitializer(llvm::cast<llvm::Constant>(irBuilder.CreateNullValue(*chirType)));
        }
        if (!chirGV->GetParentCustomTypeDef()) {
            auto fieldMeta = llvm::MDTuple::get(
                cgMod.GetLLVMContext(), {llvm::MDString::get(cgMod.GetLLVMContext(), MangleType(*chirGV->GetType()))});
            rawGV->setMetadata(GC_GLOBAL_VAR_TYPE, fieldMeta);
        }
        // When HotReload is enabled, we should put those user-defined GVs with `internal`
        // linkage into llvm.used to prevent them from being eliminated by llvm-opt.
        // Check whether the GV is user-defined based on whether it has AST info.
        if (cgMod.GetCGContext().GetCompileOptions().enableHotReload &&
            !chirGV->TestAttr(CHIR::Attribute::COMPILER_ADD) &&
            chirGV->Get<CHIR::LinkTypeInfo>() == Linkage::INTERNAL) {
            cgMod.GetCGContext().AddLLVMUsedVars(rawGV->getName().str());
        }
    }
}
#endif

void EmitGlobalVariableIR(CGModule& cgMod, const std::vector<CHIR::GlobalVar*>& chirGVs)
{
    IRGenerator<GlobalVariableGeneratorImpl>(cgMod, chirGVs).EmitIR();
}
} // namespace CodeGen
} // namespace Cangjie
