; What calls CTun's functions from assembly, by the compiler's labels.

		.section text

UseTun:
		lda CTun_miLevel,x
		jsr _CTun__Fill
		jsr _CTun__Stir
		jsr _CTun__Drain__Fi
		jsr _CTun__Drain__Fx
		jsr _CTun__Drain__Fc
		jsr _CVat__Init
		jsr _CTun__Fill
		rtl
