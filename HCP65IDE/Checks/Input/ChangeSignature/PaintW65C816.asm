; Paint's functions called by the labels the compiler gives them.

		.section text

_Painter:
		jsr _CBrush__Init
		jsr _CBrush__Mix
		jsr _Blend__Fss
		jsr _Blend__Fsss
		jsr _Next
		rtl
