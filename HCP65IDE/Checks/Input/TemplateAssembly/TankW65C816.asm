; What calls CTank's functions from assembly, by the compiler's labels.

		.section text

UseTank:
		jsr _CTank__Init__FPC6CGauge
		jsr _CTank__Level
		jsr _CGauge__Read
		rtl
