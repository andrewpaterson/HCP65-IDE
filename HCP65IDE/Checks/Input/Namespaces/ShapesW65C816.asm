; Geometry's and Drawing's functions in assembly, as the compiler labels them.

		.section text

_Geometry__CPoint__Sum:
		rtl
_Geometry__Area__FPC16Geometry__CPoint:
		jsr _Geometry__CPoint__Sum
		lda _Geometry__giCount
		rtl
_Geometry__Detail__Twice__Fi:
		rtl
_Drawing__Plot__FPC16Geometry__CPoint:
		jsr _Geometry__Detail__Twice__Fi
		rtl
_Clear:
		rtl
