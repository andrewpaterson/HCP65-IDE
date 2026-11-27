//Excluded from Debug|Win32, which defines EXCLUDED_DEBUG: compiled there, it stops the build.
#ifdef EXCLUDED_DEBUG
#error NotInDebug.cpp is excluded from Debug builds.
#endif // EXCLUDED_DEBUG


int NotInDebug(void)
{
	return 2;
}

