; Paint's functions called by the labels the compiler gives them.

		.section text

_Painter:
		jsr _CBrush__Init
		jsr _CBrush__Mix
		jsr _Blend__Fii
		jsr _Blend__Fiii
		jsr _Next
		rtl
