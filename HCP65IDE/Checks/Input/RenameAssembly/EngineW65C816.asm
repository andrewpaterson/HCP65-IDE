; CEngine's functions in assembly, as the compiler labels them.  CEngineCache keeps its own.

		.section text

_CEngine__0:
_CEngine__Init:
		jsr _CEngineCache__Init
		rtl
_CEngine__Run__Fi:
_CEngine__Run__Fc:
		rtl
_CPart__Attach__FP7CEngine:
		jsr _CEngine__Init
		rtl
_EngineCount__FP7CEngine:
		lda #<_CEngine__2
		rtl
_CEngineCache__Init:
		rtl
gszName:
		.byte "CEngine", 0
