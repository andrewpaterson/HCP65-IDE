; What CHolder's instances call, in assembly, as the compiler labels them.

		.section text

_CFoo__Init:
		rtl
_CBar__Init:
		rtl
_CLone__Init:
		rtl
_Restart:
		jsr _CFoo__Init
		jsr _CBar__Init
		jsr _CLone__Init
		rtl
