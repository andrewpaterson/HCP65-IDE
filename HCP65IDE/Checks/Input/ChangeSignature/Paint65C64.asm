; Paint's functions called by the labels the compiler gives them, for the 65C64.

		.section text

_Painter64:
		jsr _CBrush__Init
		jsr _Blend__Fii
		jsr _Blend__Fiii
		jsr _Next
		rts
