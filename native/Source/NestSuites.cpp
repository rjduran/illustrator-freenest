#include "IllustratorSDK.h"
#include "NestSuites.h"
#include "AIStringFormatUtils.h"
extern "C" {
AIMenuSuite* sAIMenu=nullptr;
AIUnicodeStringSuite* sAIUnicodeString=nullptr;
AIStringFormatUtilsSuite* sAIStringFormatUtils=nullptr;
SPBlocksSuite* sSPBlocks=nullptr;
AIMatchingArtSuite* sAIMatchingArt=nullptr;
AIMdMemorySuite* sAIMdMemory=nullptr;
AIArtSuite* sAIArt=nullptr;
AIPathSuite* sAIPath=nullptr;
AIPathStyleSuite* sAIPathStyle=nullptr;
AILayerSuite* sAILayer=nullptr;
AIUndoSuite* sAIUndo=nullptr;
AIArtboardSuite* sAIArtboard=nullptr;
AIDocumentSuite* sAIDocument=nullptr;
AIPanelSuite* sAIPanel=nullptr;
AIUIThemeSuite* sAIUITheme=nullptr;
AISymbolSuite* sAISymbol=nullptr;
AIArtStyleSuite* sAIArtStyle=nullptr;
AITransformArtSuite* sAITransformArt=nullptr;
}
ImportSuite gImportSuites[]={
 kAIMenuSuite,kAIMenuSuiteVersion,&sAIMenu,
 kAIUnicodeStringSuite,kAIUnicodeStringVersion,&sAIUnicodeString,
 kAIStringFormatUtilsSuite,kAIStringFormatUtilsSuiteVersion,&sAIStringFormatUtils,
 kSPBlocksSuite,kSPBlocksSuiteVersion,&sSPBlocks,
 kAIMatchingArtSuite,kAIMatchingArtVersion,&sAIMatchingArt,
 kAIMdMemorySuite,kAIMdMemoryVersion,&sAIMdMemory,
 kAIArtSuite,kAIArtVersion,&sAIArt,
 kAIPathSuite,kAIPathVersion,&sAIPath,
 kAIPathStyleSuite,kAIPathStyleVersion,&sAIPathStyle,
 kAILayerSuite,kAILayerVersion,&sAILayer,
 kAIUndoSuite,kAIUndoVersion,&sAIUndo,
 kAIArtboardSuite,kAIArtboardSuiteVersion,&sAIArtboard,
 kAIDocumentSuite,kAIDocumentSuiteVersion,&sAIDocument,
 kAIPanelSuite,kAIPanelSuiteVersion,&sAIPanel,
 kAIUIThemeSuite,kAIUIThemeSuiteVersion,&sAIUITheme,
 kAISymbolSuite,kAISymbolVersion,&sAISymbol,
 kAIArtStyleSuite,kAIArtStyleVersion,&sAIArtStyle,
 kAITransformArtSuite,kAITransformArtVersion,&sAITransformArt,
 nullptr,0,nullptr
};
