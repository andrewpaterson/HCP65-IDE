; NGear's and CBox's functions in assembly, as the compiler labels them: scopes and template arguments.

		.section text

_NGear__CGear__Turn:
		rtl
_NGear__Spin__FPC12NGear__CGear:
		jsr _NGear__CGear__Turn
		rtl
_CBox__T_C12NGear__CGear__Fill:
		rtl
_CBox__T_i__Fill:
		rtl
_Pack__FPC23CBox__T_C12NGear__CGear:
		jsr _CBox__T_C12NGear__CGear__Fill
		rtl
_Load__FPC9CBox__T_i:
		jsr _CBox__T_i__Fill
		rtl
_GearCount:
		jsr _Load__FPC9CBox__T_i
		rtl
_NGear__GearCount__Fi:
		jsr _GearCount
		rtl
