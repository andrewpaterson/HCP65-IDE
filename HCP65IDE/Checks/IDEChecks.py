#!/usr/bin/env python3
#The HCP65 IDE's checks: the IDE is run scripted (HCP65_IDE_* variables, see CIDEApp::Setup and CIDEScript) on the
#ModPlay and Kernal solutions together, and what it prints is checked.  The files it is pointed at (in CommonLibraries)
#must be as committed, and none of them, nor the user's layout, workspace or recent solutions, may change.
#
#  IDEChecks.py [ide executable] [-native] [-outputs <directory>] [-open <file>...] [check name...]
#
#The checks run on the IDE drawn on the reference screen (HCP65IDERef, see HCP65IDERef.cpp), in memory: no display is
#needed and nothing typed or clicked reaches it.  Then, unless checks are named, the native smoke checks (NATIVE_SMOKE)
#run on the IDE in a real window, on X11 or Windows, so its backend is run too.  -native runs every check on the native
#IDE instead.  The executables default to the Release builds (REFERENCE_EXECUTABLE and NATIVE_EXECUTABLE); one given
#runs every check.  Exits non-zero if a check fails.  -outputs <directory> writes what each check printed to
#<directory>/<check>.txt (<check>.native.txt for a smoke check), to compare runs.
#
#With HCP65_IDE_VERIFY_REDRAW set, every partial paint is checked too, and one that differed fails its check.
#With HCP65_IDE_VERIFY_LAYOUT set, the window is laid out every paint, and a component that had moved without the
#layout being invalidated fails its check.
#With HCP65_IDE_VERIFY_PRESENT set, on the reference screen every present is checked: what a window shows after it must
#be what it drew, and one that isn't fails its check.
#
#What the checks open doesn't depend on what the user opened last, and doesn't change it: each run is given recent
#solutions of its own (HCP65_IDE_RECENT, a file made for the checks) that say what was open.  By default that is the
#user's ModPlay workspace (HCP65/ModPlay/HCP65ModPlay.hcp65workspace: ModPlay, the startup solution, and the kernal it
#runs on), as it is now.  -open <file> opens something else: a workspace, a solution, or several solutions (-open for
#each: they are opened together, the first the startup one, as a workspace made for the checks); -open user opens
#whatever the user had open last, as the IDE itself would.  The user's own recent solutions, and the workspace's file
#and layout, are put back as they were when the checks end if anything changed them.
import atexit, hashlib, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
IDE_DIR = os.path.join(HERE, '..', 'HCP65IDE')
#Rename's searches are checked on a project of their own (Input/RenameSearch), whose counts don't move as other code does.
RENAME_SEARCH = os.path.join(HERE, 'Input', 'RenameSearch', 'RenameSearch.vcxproj')
#Rename File is checked on a solution of its own (Input/RenameFile): Engine.h, the #includes that find it and some that don't.
RENAME_FILE = os.path.join(HERE, 'Input', 'RenameFile', 'RenameFile.sln')
#The navigation commands are checked on a project of their own (Input/Navigation): CShape, with virtual Draw and Area,
#the CCircle and CSquare derived from it, CRoundedSquare from CSquare, and a Main.cpp that uses them.  Nothing is
#changed in it.
NAVIGATION = os.path.join(HERE, 'Input', 'Navigation', 'Navigation.vcxproj')
QUICK_INFO = os.path.join(HERE, 'Input', 'QuickInfo', 'QuickInfo.vcxproj')
BASES = os.path.join(HERE, 'Input', 'Bases', 'Bases.vcxproj')
#A unit including <windows.h> (Input/WindowsHeaders), with Win32 and x64 configurations.
WINDOWS_HEADERS = os.path.join(HERE, 'Input', 'WindowsHeaders', 'WindowsHeaders.vcxproj')
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
BASELIB = os.path.join(ROOT, 'CommonLibraries', 'Libraries', 'BaseLib')
KERNAL = os.path.join(ROOT, 'HCP65', 'Kernal')
MODPLAY = os.path.join(ROOT, 'HCP65', 'ModPlay')
USER_RECENT = os.path.expanduser('~/.config/HCP65IDE/RecentSolutions.xml')
if os.name == 'nt':
	#The Visual Studio builds: Release|x64 and Reference|x64 (HCP65IDERef.cpp in place of the WinMain).
	REFERENCE_EXECUTABLE = os.path.join(HERE, '..', 'x64', 'Reference', 'HCP65IDERef.exe')
	NATIVE_EXECUTABLE = os.path.join(HERE, '..', 'x64', 'Release', 'HCP65IDE.exe')
else:
	REFERENCE_EXECUTABLE = os.path.join(ROOT, 'build', 'HCP65IDERelease', 'HCP65IDERef')
	NATIVE_EXECUTABLE = os.path.join(ROOT, 'build', 'HCP65IDERelease', 'HCP65IDE')
#The checks also run on the native IDE, in a real window, by default: as few as show its backend works.
NATIVE_SMOKE = ['explorer']

#Files the checks edit in the IDE (never saving them) and the user's files a scripted run mustn't write.
WATCHED = [os.path.join(BASELIB, f) for f in ['CalculatorParser.cpp', 'CalculatorParser.h', 'CalculatorSymbols.cpp', 'CalcParentheses.h', 'CalcExpression.h']] + \
          [os.path.join(KERNAL, 'HCP65Kernal.layout.user'), os.path.join(KERNAL, 'HCP65Kernal.hcp65workspace'), os.path.join(KERNAL, 'HCP65Kernal.hcp65build'),
           os.path.join(MODPLAY, 'HCP65ModPlay.layout.user'), os.path.join(MODPLAY, 'HCP65ModPlay.hcp65workspace'), os.path.join(MODPLAY, 'HCP65ModPlay.hcp65build'),
           USER_RECENT, os.path.expanduser('~/.config/HCP65IDE/Keymap.xml')]
#The user's files that are put back as they were when the checks end, if anything changed them.
RESTORED = [USER_RECENT] + [f for f in WATCHED if f.endswith('.layout.user') or f.endswith('.hcp65workspace')]
#The recent solutions each run is given, unless its check has its own: made by OpenForChecks, None for the user's.
CHECKS_RECENT = None
BREAKPOINTS = os.path.join(KERNAL, 'HCP65Kernal.hcp65user')
#Where what each check printed is written (-outputs), None for nowhere.
OUTPUTS = None



def MakeRecentSolutions(aszFiles):
	#A recent solutions file, made for the checks, that says aszFiles was open last: a workspace or a solution, or
	#several solutions as a workspace made for the checks (the first the startup one).
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	aszFiles = [os.path.abspath(szFile) for szFile in aszFiles]
	for szFile in aszFiles:
		if not os.path.isfile(szFile):
			sys.exit('IDEChecks: %s: no such file to open.' % szFile)
	szOpen = aszFiles[0]
	if len(aszFiles) > 1:
		szOpen = os.path.join(szRoot, 'Checks.hcp65workspace')
		szText = '<?xml version="1.0" encoding="UTF-8"?>\n<Workspace>\n'
		for i, szFile in enumerate(aszFiles):
			szText += '  <Solution File="%s"%s/>\n' % (szFile, ' Startup="true"' if i == 0 else '')
		open(szOpen, 'w', newline='').write(szText + '</Workspace>\n')
	szRecent = os.path.join(szRoot, 'RecentSolutions.xml')
	szText = '<?xml version="1.0" encoding="UTF-8"?>\n<RecentFiles Open="%s">\n  <File Path="%s"/>\n</RecentFiles>\n' % (szOpen, szOpen)
	open(szRecent, 'w', newline='').write(szText)
	return szRecent


def OpenForChecks(aszFiles):
	#What every check that hasn't its own opens: aszFiles (see MakeRecentSolutions), or with ['user'] whatever the user
	#had open last.
	global CHECKS_RECENT
	CHECKS_RECENT = None if (aszFiles == ['user']) else MakeRecentSolutions(aszFiles)


def OpenedForCheck(aszFiles):
	#For one check: recent solutions of its own that say aszFiles was open last, as OpenForChecks makes for them all.
	szRecent = MakeRecentSolutions(aszFiles)
	return {'RECENT': szRecent}


#The checks that wait for BaseLib's index and look at its Calc files open a small solution of just those files
#(Checks/Input/Calculator: a project named BaseLib with the real files by relative path), not ModPlay: indexing
#all of BaseLib was 90% of each one's time (CommonLibraries Tests/TestSpeedPlan.txt step 2).  Those about indexing at
#ModPlay's scale (rename-while-reindexing, recolour-off-ui-thread, usages-typing, usages-step-behind-build), and those
#whose results are ModPlay's (gotosource-popup-scrolls, find-jump-to-source, find-preview, find-row-parts,
#find-in-solution), still open what the run opens.
CALCULATOR = os.path.join(HERE, 'Input', 'Calculator', 'Calculator.sln')
CALCULATOR_RECENT = MakeRecentSolutions([CALCULATOR])


def RestoreUserFiles(asContents):
	#The user's recent solutions, workspaces and layouts as they were when the checks started (asContents: each file's
	#bytes, None if it wasn't there), put back if they aren't.
	for szFile, sContents in asContents.items():
		sNow = open(szFile, 'rb').read() if os.path.exists(szFile) else None
		if sNow != sContents:
			if sContents is None:
				os.remove(szFile)
			else:
				open(szFile, 'wb').write(sContents)
			print('Put back %s' % szFile)


def MakeSystemRoot(aszVersions, bLibrary=True):
	#A folder laid out as a machine with these MinGW GCC folders (and their C library's headers) is, for HCP65_IDE_SYSTEM_ROOT.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	for szVersion in aszVersions:
		os.makedirs(os.path.join(szRoot, 'usr/lib/gcc/x86_64-w64-mingw32', szVersion, 'include/c++/x86_64-w64-mingw32'))
	if bLibrary:
		os.makedirs(os.path.join(szRoot, 'usr/share/mingw-w64/include'))
	return szRoot


def MakeSolutionCopy(bOther=False):
	#A copy of a test solution with a comment in its build configurations, to save them to: a scripted run told to keep
	#its settings with SAVE_LAYOUT, and its recent solutions apart, writes only beside the copy.  It can have a second
	#build configuration (bOther), Other|W65C816, for when one of them is removed.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szKernel = os.path.join(szRoot, 'Workspace', 'Kernel')
	shutil.copytree(os.path.join(ROOT, 'HCP65-IDE', 'Tests', 'TestBuildLib', 'TestBuildLib', 'Input', 'Workspace'), os.path.join(szRoot, 'Workspace'))
	szFile = os.path.join(szKernel, 'Kernel.hcp65build')
	szText = open(szFile, newline='').read().replace('<BuildConfigurations>', '<!-- A comment to keep. -->\n<BuildConfigurations>', 1)
	if bOther:
		szText = szText.replace('</BuildConfigurations>', '\t<Configuration Name="Other|W65C816" Toolchain="W65C816" Solution="Release|Win32"/>\n</BuildConfigurations>', 1)
	open(szFile, 'w', newline='').write(szText)
	return {'SOLUTION': os.path.join(szKernel, 'Kernel.sln'), 'SAVE_LAYOUT': '1', 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeToolsCopy():
	#A copy of the test solutions opened on Tools, which has two applications (First and Second) and a library (Shared).
	asCopy = MakeSolutionCopy()
	asCopy['SOLUTION'] = os.path.join(os.path.dirname(os.path.dirname(asCopy['SOLUTION'])), 'Tools', 'Tools.sln')
	return asCopy


#The debugger's windows are checked on a program of their own (Input/Debugging: a class, a recursive Sum, an array, a
#char*, an enum and a float), on both processors.  The W65C816's (Debugging.sln) is built and run on the HCP65 in the
#emulator drawn on the reference screen (HCP65EmulatorRef, built beside HCP65Emulator), as the start up program of a copy
#of TestHCP65Lib's boot card: TestHCP65Lib must have been run, as the card, the boot loader and OS.SYS are its.  The
#65C64's (Debugging65C64.sln, the same Debugging.cpp with Libc65's Start65C64.asm) is an image run from reset in the X65C64
#Emulator, a console program (build/X65C64Emulator).
DEBUGGING = os.path.join(HERE, 'Input', 'Debugging')
DEBUGGING_CARD = os.path.join(ROOT, 'HCP65', 'HCP65-Emulator', 'TestHCP65Lib', 'TestHCP65Lib', 'Output', 'BootCard.img')
if os.name == 'nt':
	REFERENCE_EMULATOR = os.path.join(ROOT, 'HCP65', 'HCP65-Emulator', 'x64', 'Release', 'HCP65EmulatorRef.exe')
	X65C64_EMULATOR = os.path.join(ROOT, 'HCP65', 'X65C64-Emulator', 'x64', 'Release', 'X65C64Emulator.exe')
else:
	REFERENCE_EMULATOR = os.path.join(ROOT, 'build', 'HCP65EmulatorRelease', 'HCP65EmulatorRef')
	X65C64_EMULATOR = os.path.join(ROOT, 'build', 'X65C64Emulator', 'X65C64Emulator')


def CleanDebugging():
	#What building and debugging Input/Debugging leaves: its build, the card, and the breakpoints and watches it saved.
	for szName in ['Output', 'Listing']:
		shutil.rmtree(os.path.join(DEBUGGING, szName), True)
	for szName in os.listdir(DEBUGGING):
		if szName.endswith('.hcp65user') or szName.endswith('.layout.user'):
			os.remove(os.path.join(DEBUGGING, szName))


def PrepareDebugging():
	#Input/Debugging as from nothing, before each run: no build, no breakpoints or watches saved, and its card a fresh
	#copy of the boot card (a sparse file: copied sparsely, it is small).
	CleanDebugging()
	os.makedirs(os.path.join(DEBUGGING, 'Output'))
	szCard = os.path.join(DEBUGGING, 'Output', 'Debugging.img')
	if os.name == 'nt':
		shutil.copyfile(DEBUGGING_CARD, szCard)
	else:
		subprocess.run(['cp', '--sparse=always', DEBUGGING_CARD, szCard], check=True)


#What the debugging checks differ in on each processor: the check names' suffix, the solution, the emulator (the
#HCP65_IDE_ variable naming it) and what makes a run's start, then what is printed: the program's module, a pointer
#into its data and into its text, a stopped address, an int in hexadecimal, and its Release configuration.
DEBUGGING_W65C816 = {'SUFFIX': '', 'SOLUTION': 'Debugging.sln', 'EMULATOR_VARIABLE': 'EMULATOR', 'EMULATOR': REFERENCE_EMULATOR,
	'PREPARE': PrepareDebugging, 'MODULE': r'debugging\.exe', 'POINTER': r'0x00[0-9a-f]{4}', 'TEXT_POINTER': r'0x[0-9a-f]{6}',
	'ADDRESS': r'\$[0-9A-F]{2}:[0-9A-F]{4}', 'HEX_INT': '0x0000', 'RELEASE': 'Release|W65C816'}
DEBUGGING_65C64 = {'SUFFIX': '-65C64', 'SOLUTION': 'Debugging65C64.sln', 'EMULATOR_VARIABLE': '65C64_EMULATOR', 'EMULATOR': X65C64_EMULATOR,
	'PREPARE': CleanDebugging, 'MODULE': r'debugging\.bin', 'POINTER': r'0x[0-9a-f]{8}', 'TEXT_POINTER': r'0x[0-9a-f]{8}',
	'ADDRESS': r'\$[0-9A-F]{8}', 'HEX_INT': '0x00000000', 'RELEASE': 'Release|65C64'}


def MakeDebugging(asProcessor, szDebug, szConfiguration=None):
	#A run debugging Input/Debugging for asProcessor, built first, in its emulator, with the debugger's script szDebug
	#(after choosing szConfiguration, if given).  Each run starts from nothing (PREPARE), and what it leaves goes at the end.
	atexit.register(CleanDebugging)
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	if szConfiguration:
		#The build at the start is of the configuration chosen then: the one asked for is built after it.
		szDebug = 'configuration:' + szConfiguration + ',command:Build.Build,wait:300,built,' + szDebug
	return {'SOLUTION': os.path.join(DEBUGGING, asProcessor['SOLUTION']), 'RECENT': os.path.join(szRoot, 'Recent.xml'), 'OPEN': 'Debugging.cpp', 'BUILD': '1',
			asProcessor['EMULATOR_VARIABLE']: asProcessor['EMULATOR'], 'DEBUG': 'built,' + szDebug, 'PREPARE': asProcessor['PREPARE']}


def DebugCallStack(asProcessor):
	#Sum's recursion stopped at its end (line 37, iDepth 0): each call with its parameters and line, main's, and the
	#start up's code, which has no debug records.
	szSum = '^   ' + asProcessor['MODULE'] + r'!Sum\(CCounter\* pcCounter = ' + asProcessor['POINTER']
	return [r'^Call Stack:$', '^=> ' + asProcessor['MODULE'] + r'!Sum\(CCounter\* pcCounter = ' + asProcessor['POINTER'] + r', int iDepth = 0\) Line 37$',
		szSum + r', int iDepth = 1\) Line 39$', szSum + r', int iDepth = 4\) Line 39$',
		'^   ' + asProcessor['MODULE'] + r'!main\(\) Line 53$', r'^   \[External Code\]$']



def DebuggingChecks(P):
	#The debugger's checks on Input/Debugging built for processor P (DEBUGGING_W65C816, DEBUGGING_65C64): written once,
	#run on each.
	return [
	('debug-call-stack' + P['SUFFIX'], MakeDebugging(P, 'line:37,start,stopped,wait:1500,printstopped,printcallstack,print:Locals,stop'),
		#Stopped at a breakpoint: Call Stack's calls, Locals the innermost's parameters and locals.
		['^Stopped: yes, ' + P['ADDRESS'] + r', .*Debugging\.cpp:37$'] + DebugCallStack(P) +
		[r'^Locals:$', r'^pcCounter = ' + P['POINTER'] + r' \[CCounter\*\]$', r'^iDepth = 0 \[int\]$', r'^iBelow = 0 \[int\]$'], [r'\?\?\?', r' \*$']),
	('debug-frames' + P['SUFFIX'], MakeDebugging(P, 'line:37,start,stopped,wait:1500,frame:5,wait:500,printcallstack,print:Locals,open:Locals:cCounter,wait:300,print:Locals,'
		'frame:3,wait:500,print:Locals,print:Autos'),
		#A call chosen in Call Stack: its arrow, its locals, an object opened (an enum, a float); Autos of its line.
		[r'^Call Stack:$', r'^=> ' + P['MODULE'] + r'!Sum\(.*iDepth = 0\) Line 37$', r'^-> ' + P['MODULE'] + r'!main\(\) Line 53$',
		 r'^Locals:$', r'^cCounter = \{\.\.\.\} \[CCounter\]$', r'^iTotal = 0 \[int\]$',
		 r'^Locals:$', r'^cCounter = \{\.\.\.\} \[CCounter\]$', r'^  miCount = 0 \[int\]$', r'^  meColour = EC_Green \[EColour\]$', r'^  mfScale = 1\.5 \[float\]$',
		 r'^Locals:$', r'^pcCounter = ', r'^iDepth = 3 \[int\]$', r'^Autos:$', r'^iBelow = ', r'^pcCounter = ', r'^iDepth = 3 \[int\]$'], [r'\?\?\?']),
	('debug-watch' + P['SUFFIX'], MakeDebugging(P, 'line:37,start,stopped,wait:1500,watch:1:gaiValues,watch:1:gszName,watch:1:pcCounter->meColour,watch:1:gaiValues[iDepth + 2] + 1,'
		'watch:1:nope,watch:2:gfScale,wait:500,open:Watch1:gaiValues,wait:300,print:Watch1,print:Watch2,hex,wait:300,print:Locals,hex,'
		'set:Watch1:gaiValues/[3]:9,set:Watch1:gaiValues/[2]:x,wait:500,print:Watch1,printuserfile'),
		#Watches typed in: values, an array opened, errors; Hexadecimal Display; a value written; kept in the .hcp65user.
		[r'^Watch1:$', r'^gaiValues = \{\.\.\.\} \[int\[4\]\]$', r'^  \[0\] = 3 \[int\]$', r'^  \[3\] = 1 \[int\]$', r'^gszName = ' + P['TEXT_POINTER'] + r' "Debugging" \[const char\*\]$',
		 r'^pcCounter->meColour = EC_Green \[EColour\]$', r'^gaiValues\[iDepth \+ 2\] \+ 1 = 5 \[int\]$', r'^nope = nope is undefined \[\]$', r'^ =  \[\]$',
		 r'^Watch2:$', r'^gfScale = 1\.5 \[float\]$', r'^Locals:$', r'^iDepth = ' + P['HEX_INT'] + r' \[int\]$', r'^Not written: gaiValues/\[2\] = x$',
		 r'^Watch1:$', r'^  \[3\] = 9 \[int\]$', r'<Watch Window="1" Expression="gaiValues"/>', r'<Watch Window="2" Expression="gfScale"/>'], [r'\?\?\?']),
	('debug-autos' + P['SUFFIX'], MakeDebugging(P, 'line:37,start,stopped,wait:1500,out,stopped,wait:1500,printstopped,printcallstack,print:Autos,over,stopped,wait:1000,print:Locals'),
		#Stepped out: what Sum returned in Autos, with what its caller's line and the line before use; then a step over,
		#and the value it changed red.
		[r'^Stopped: yes, .*Debugging\.cpp:39$', r'^=> ' + P['MODULE'] + r'!Sum\(.*iDepth = 1\) Line 39$', r'^Autos:$', r'^Sum returned = 0 \[int\]$', r'^iBelow = 0 \[int\]$',
		 r'^pcCounter = ', r'^iDepth = 1 \[int\]$', r'^Locals:$', r'^iDepth = 1 \[int\]$'], [r'\?\?\?']),
	('debug-data-tip' + P['SUFFIX'], MakeDebugging(P, 'line:37,start,stopped,wait:1500,datatip:40:18,wait:300,print:DataTip,open:DataTip:gaiValues,wait:300,print:DataTip,'
		'datatip:40:5,print:DataTip,datatip:31:1'),
		#The data tip of a name in the editor, opened; one of a member chain (pcCounter->Add's pcCounter); none on a keyword.
		[r'^DataTip:$', r'^gaiValues = \{\.\.\.\} \[int\[4\]\]$', r'^DataTip:$', r'^gaiValues = ', r'^  \[0\] = 3 \[int\]$', r'^  \[3\] = 1 \[int\]$',
		 r'^DataTip:$', r'^pcCounter = ' + P['POINTER'] + r' \[CCounter\*\]$', r'^No data tip at 31:1$'], []),
	('debug-quick-watch' + P['SUFFIX'], dict(MakeDebugging(P, 'line:37,start,stopped,wait:1500,edit:caret:40:18,command:Debug.QuickWatch,answered:2,print:Watch1'), **{'DIALOG_ANSWER': 'Reevaluate|OK'}),
		#QuickWatch of the expression at the caret: Reevaluate shows it again; Add Watch (its OK) puts it in Watch 1.
		[r'^Dialog \[QuickWatch\] answered Reevaluate$', r'^Dialog \[QuickWatch\] answered OK$', r'^Watch1:$', r'^gaiValues = \{\.\.\.\} \[int\[4\]\]$'], []),
	('debug-data-breakpoint' + P['SUFFIX'], MakeDebugging(P, 'line:53,start,stopped,wait:1500,open:Locals:cCounter,wait:300,databreak:Locals:cCounter/miCount,databreak:Locals:iTotal,'
		'databreak:Locals:iTotal,line:53,continue,stopped,wait:1500,printstopped,continue,stopped,wait:1500,printstopped,databreak:Locals:nope'),
		#Break When Value Changes on a member (iTotal's put on and taken off again): it stops in CCounter::Add after the
		#store, saying what the value was and is, twice; a value that isn't shown can't be watched.
		[r'^Stopped: yes, ' + P['ADDRESS'] + r', .*Debugging\.cpp:2[78]$', r'^Debug status: Data breakpoint: cCounter\.miCount changed from 0 to 3 at ',
		 r'^Stopped: yes, ' + P['ADDRESS'] + r', .*Debugging\.cpp:2[78]$', r'^Debug status: Data breakpoint: cCounter\.miCount changed from 3 to 4 at ',
		 r'^No data breakpoint: nope$'], [r'iTotal changed', r'\?\?\?']),
	('debug-new-data-breakpoint' + P['SUFFIX'], dict(MakeDebugging(P, 'line:53,start,stopped,wait:1500,command:Debug.NewDataBreakpoint,answered:1,line:53,continue,stopped,wait:1500,printstopped'),
		**{'DIALOG_KEYS': 'type:cCounter.miCount', 'DIALOG_ANSWER': 'OK'}),
		#New Data Breakpoint on an expression's value, in the frame shown.
		[r'^Dialog \[New Data Breakpoint\] answered OK$', r'^Debug status: Data breakpoint: cCounter\.miCount changed from 0 to 3 at '], [r'\?\?\?']),
	('debug-inlined-step-out' + P['SUFFIX'], MakeDebugging(P, 'line:39,start,stopped,wait:1500,printcallstack,line:39,out,stopped,wait:1500,printstopped,printcallstack', P['RELEASE']),
		#Built optimised, stopped in Sum's first call, inlined into main (DebuggerFPlan.txt F2-5): Step Out leaves the
		#inlined call's code, its own call of Sum stepped over, back in main.
		[r'^Call Stack:$', r'^=> ' + P['MODULE'] + r'!Sum\(.*\) Line 39 \[inlined\]$', r'^   ' + P['MODULE'] + r'!main\(\) Line 53$',
		 r'^Stopped: yes, .*Debugging\.cpp:5[34]$', r'^Call Stack:$', r'^=> ' + P['MODULE'] + r'!main\(\) Line 5[34]$'], [r'\?\?\?']),
	('debug-inlined-step-over' + P['SUFFIX'], MakeDebugging(P, 'line:39,start,stopped,wait:1500,line:39,over,stopped,wait:1500,printstopped,over,stopped,wait:1500,printstopped,printcallstack', P['RELEASE']),
		#Built optimised, stopped in Sum's first call, inlined into main: Step Over line 39, its call of Sum run to its end
		#(the recursion runs line 39 again: DebuggerFixesPlan.txt G2), then over line 40 with CCounter::Add inlined into
		#it (DebuggerFPlan.txt F2-5), to line 41 of the same inlined call: never in Add.
		[r'^Stopped: yes, .*Debugging\.cpp:40$', r'^Stopped: yes, .*Debugging\.cpp:41$', r'^Call Stack:$', r'^=> ' + P['MODULE'] + r'!Sum\(.*\) Line 41 \[inlined\]$',
		 r'^   ' + P['MODULE'] + r'!main\(\) Line 53$'],
		[r'\?\?\?', r'Debugging\.cpp:2[78]$']),
	('debug-optimised' + P['SUFFIX'], MakeDebugging(P, 'line:39,start,stopped,wait:1500,printcallstack,continue,stopped,wait:1500,printcallstack,line:39,line:54,continue,stopped,wait:1500,printstopped,print:Locals', P['RELEASE']),
		#Built optimised, Sum's first call inlined into main (which the optimiser may choose otherwise): it is a row of
		#its own in the Call Stack (DebuggerFPlan.txt F2), main's below it at the call.  Sum's parameters are where
		#their values are (registers or the frame, F1): the second stop is Sum(&cCounter, 3), called from the inlined
		#Sum.  Back in main, what stays in memory (the object whose address went to Sum) is shown, and so is iTotal where
		#the optimiser kept it: 3 + 1 + 4 + 1.
		[r'^Call Stack:$', r'^=> ' + P['MODULE'] + r'!(main\(\)|Sum\(.*\)) Line 39( \[inlined\])?$', r'^   \[External Code\]$',
		 r'^Call Stack:$', r'^=> ' + P['MODULE'] + r'!Sum\(CCounter\* pcCounter = 0x[0-9a-f]+, int iDepth = 3\) Line 39$',
		 r'^   ' + P['MODULE'] + r'!(main\(\)|Sum\(.*\)) Line [0-9]+( \[inlined\])?$', r'^   \[External Code\]$',
		 r'^Stopped: yes, .*Debugging\.cpp:54$', r'^Locals:$', r'^cCounter = \{\.\.\.\} \[CCounter\]$', r'^iTotal = 9 \[int\]$'], [r'\?\?\?'])]


def MakeBrokenKernel():
	#A copy of the test solution whose kernel doesn't compile: two errors in Kernel.cpp, on lines 2 and 5, and a usage of
	#F on line 5.  Its recent solutions are apart from the user's.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szKernel = os.path.join(szRoot, 'Workspace', 'Kernel')
	shutil.copytree(os.path.join(ROOT, 'HCP65-IDE', 'Tests', 'TestBuildLib', 'TestBuildLib', 'Input', 'Workspace'), os.path.join(szRoot, 'Workspace'))
	open(os.path.join(szKernel, 'Kernel.cpp'), 'w', newline='').write('int main(void) { return 0; }\nint F(void) { return giMissing; }\nint G(void)\n{\n\treturn F() + giAlsoMissing;\n}\n')
	return {'SOLUTION': os.path.join(szKernel, 'Kernel.sln'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeIndexingProblemsKernel():
	#A copy of the test solution whose kernel's indexing has problems: Kernel.cpp includes Defs.h, which defines V again
	#differently on its line 2, and Gone.h, which isn't there, on its line 2.  Its recent solutions are apart from the
	#user's.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szKernel = os.path.join(szRoot, 'Workspace', 'Kernel')
	shutil.copytree(os.path.join(ROOT, 'HCP65-IDE', 'Tests', 'TestBuildLib', 'TestBuildLib', 'Input', 'Workspace'), os.path.join(szRoot, 'Workspace'))
	open(os.path.join(szKernel, 'Kernel.cpp'), 'w', newline='').write('#include "Defs.h"\n#include "Gone.h"\nint main(void) { return V; }\n')
	open(os.path.join(szKernel, 'Defs.h'), 'w', newline='').write('#define V 1\n#define V 2\n')
	return {'SOLUTION': os.path.join(szKernel, 'Kernel.sln'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


USAGES_KERNEL = ('int F(void) { return 0; }\n'
                 'class CA { public: int M(void) { return F(); } int N(void); };\n'
                 'int G(void) { return F(); }\n'
                 'int CA::N(void) { return F(); }\n'
                 'int main(void) { return F() + G(); }\n')


def ManyUsages(szPrefix, iCount):
	#A file of iCount functions, <prefix>0 on line 2 to <prefix><iCount - 1>, each using F once.
	return 'int F(void);\n' + ''.join('int %s%d(void) { return F(); }\n' % (szPrefix, i) for i in range(iCount))


def MakeUsagesKernel(szKernel=USAGES_KERNEL, asFiles={}):
	#A copy of the test solution whose kernel is szKernel: as it is given, F is defined on line 1 and used on lines 2 and
	#4, in CA, and on lines 3 and 5, in no class.  asFiles are more of the kernel project's files, by name.  Its recent
	#solutions are apart from the user's.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Workspace', 'Kernel')
	shutil.copytree(os.path.join(ROOT, 'HCP65-IDE', 'Tests', 'TestBuildLib', 'TestBuildLib', 'Input', 'Workspace'), os.path.join(szRoot, 'Workspace'))
	open(os.path.join(szFolder, 'Kernel.cpp'), 'w', newline='').write(szKernel)
	szItems = ''
	for szName, szText in asFiles.items():
		os.makedirs(os.path.dirname(os.path.join(szFolder, szName)), exist_ok=True)
		open(os.path.join(szFolder, szName), 'w', newline='').write(szText)
		szItems += '\r\n    <%s Include="%s" />' % ('ClCompile' if szName.endswith('.cpp') else 'ClInclude', szName.replace('/', '\\'))
	szProject = os.path.join(szFolder, 'Kernel.vcxproj')
	szText = open(szProject, newline='', encoding='utf-8-sig').read().replace('<ClCompile Include="Kernel.cpp" />', '<ClCompile Include="Kernel.cpp" />' + szItems, 1)
	open(szProject, 'w', newline='', encoding='utf-8-sig').write(szText)
	return {'SOLUTION': os.path.join(szFolder, 'Kernel.sln'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


#The Problems tool window's checks' kernel: line 3's override isn't declared override (a warning), line 4 uses a name
#not declared and line 5 is missing an expression (errors); Problems.h, which it includes, uses a name not declared too.
PROBLEMS_KERNEL = ('#include "Problems.h"\n'
                   'class CBase { public: virtual int Get(void) { return 1; } };\n'
                   'class CDerived : public CBase { public: int Get(void) { return 2; } };\n'
                   'int F(void) { return giMissing; }\n'
                   'int G(void) { return 1 +; }\n'
                   'int main(void) { return F() + G() + H(); }\n')
PROBLEMS_HEADER = 'int H(void) { return giMissingInHeader; }\n'


def MakeProblemsKernel():
	return MakeUsagesKernel(PROBLEMS_KERNEL, {'Problems.h': PROBLEMS_HEADER})


def MakeNavigationCopy(szUser=None):
	#A copy of the navigation checks' project (Input/Navigation), for checks that write beside the solution: its
	#user's file (bookmarks, recent files).  Its recent solutions are apart from the user's.  szUser is what its user's
	#file has to begin with, between its root's tags, with {folder} for the copy's folder.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Navigation')
	shutil.copytree(os.path.join(HERE, 'Input', 'Navigation'), szFolder)
	if szUser is not None:
		open(os.path.join(szFolder, 'Navigation.hcp65user'), 'w').write('<?xml version="1.0" encoding="UTF-8"?>\n<UserSettings>\n' + szUser.replace('{folder}', szFolder) + '</UserSettings>\n')
	return {'SOLUTION': os.path.join(szFolder, 'Navigation.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeRenameFileCopy():
	#A copy of Rename File's solution (Input/RenameFile), which a check can rename files in.  Its recent solutions are
	#apart from the user's.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'RenameFile')
	shutil.copytree(os.path.join(HERE, 'Input', 'RenameFile'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'RenameFile.sln'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeRenameDigitCopy():
	#A copy of Input/RenameDigit (C65Core in 65Core.h and 65Core.cpp, C65CoreCycles in 65CoreCycles.h), to rename in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'RenameDigit')
	shutil.copytree(os.path.join(HERE, 'Input', 'RenameDigit'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'RenameDigit.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeRenameAssemblyCopy():
	#A copy of Input/RenameAssembly (CEngine, its W65C816 assembly naming it as the compiler labels it), to rename in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'RenameAssembly')
	shutil.copytree(os.path.join(HERE, 'Input', 'RenameAssembly'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'RenameAssembly.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeNamespacesCopy():
	#A copy of Input/Namespaces (Geometry's CPoint, TLength, Area, giCount and Detail::Twice; Drawing::Plot; Main.cpp
	#names them through a using-directive, a using-declaration and qualified names), to move things between namespaces.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Namespaces')
	shutil.copytree(os.path.join(HERE, 'Input', 'Namespaces'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Namespaces.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeMoveAssemblyCopy():
	#A copy of Input/MoveAssembly (CTun, which derives from CVat, and W65C816 assembly that names CTun's members by
	#the compiler's labels), to move members in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'MoveAssembly')
	shutil.copytree(os.path.join(HERE, 'Input', 'MoveAssembly'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'MoveAssembly.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeTemplateAssemblyCopy():
	#A copy of Input/TemplateAssembly: CTank, which has a CGauge, and W65C816 assembly that calls CTank's functions.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'TemplateAssembly')
	shutil.copytree(os.path.join(HERE, 'Input', 'TemplateAssembly'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'TemplateAssembly.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeRenameTemplateCopy():
	#A copy of Input/RenameTemplate (CHolder<M>, whose body calls mp->Init() and reads mp->miCount, instantiated with CFoo
	#and with CBar; CLone has the same members and is in no instance), to refactor in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'RenameTemplate')
	shutil.copytree(os.path.join(HERE, 'Input', 'RenameTemplate'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'RenameTemplate.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeSafeDeleteCopy():
	#A copy of Input/SafeDelete (CShape and what uses some of it, CLonely which nothing uses, an enum, a typedef, functions
	#and a global, and assembly that calls CShape::Kill), to delete in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'SafeDelete')
	shutil.copytree(os.path.join(HERE, 'Input', 'SafeDelete'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'SafeDelete.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeIntroduceVariableCopy():
	#A copy of Input/IntroduceVariable (CCalc, whose Total has expressions written more than once, and CStack<M>), to
	#introduce variables in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'IntroduceVariable')
	shutil.copytree(os.path.join(HERE, 'Input', 'IntroduceVariable'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'IntroduceVariable.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeChangeSignatureCopy():
	#A copy of Input/ChangeSignature (CBrush, whose functions have default values, an override in CSpray and other
	#names for their parameters where they are defined; overloads of Blend; Next, which is also pointed to), to
	#change signatures in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'ChangeSignature')
	shutil.copytree(os.path.join(HERE, 'Input', 'ChangeSignature'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'ChangeSignature.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def WriteSchemeTemplates(szFolder, asTemplates):
	#A Workspace scheme of file and code templates beside a solution: asTemplates are (tab, name, text), or for a Files
	#template (tab, name, text, extension, file name), each a .ft in szFolder's fileTemplates and listed in its
	#Templates.xml.  With none there is no scheme.
	if not asTemplates:
		return
	szTemplates = os.path.join(szFolder, 'fileTemplates')
	szIndex = '<?xml version="1.0" encoding="utf-8"?>\n<FileTemplates>\n'
	for asTemplate in asTemplates:
		szTab, szName, szText = asTemplate[0], asTemplate[1], asTemplate[2]
		szExtension = asTemplate[3] if len(asTemplate) > 3 else ''
		os.makedirs(os.path.join(szTemplates, szTab), exist_ok=True)
		open(os.path.join(szTemplates, szTab, szName + ('.' + szExtension if szExtension else '') + '.ft'), 'w', newline='').write(szText)
		if len(asTemplate) > 3:
			szIndex += '\t<Template Tab="%s" Name="%s" Extension="%s" FileName="%s"/>\n' % (szTab, szName, szExtension, asTemplate[4])
		else:
			szIndex += '\t<Template Tab="%s" Name="%s"/>\n' % (szTab, szName)
	open(os.path.join(szTemplates, 'Templates.xml'), 'w', newline='').write(szIndex + '</FileTemplates>\n')


def MakeDefinitionsCopy(asTemplates=[]):
	#A copy of Input/Definitions (CTank, with functions declared and not defined, defined in the class, and defined
	#outside it; CStore, a class template; functions outside any class), to make declarations and definitions in.
	#asTemplates are its Workspace scheme's templates (WriteSchemeTemplates).
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Definitions')
	shutil.copytree(os.path.join(HERE, 'Input', 'Definitions'), szFolder)
	WriteSchemeTemplates(szFolder, asTemplates)
	return {'SOLUTION': os.path.join(szFolder, 'Definitions.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeMoveMembersCopy(szLicence=None, asTemplates=[]):
	#A copy of Input/MoveMembers (CVessel and the CDrum and CKeg that derive from it, with functions defined in and
	#outside their classes, fields, and what uses what; CBox and CCrate, class templates), to move members in.  Its
	#licence is None (MoveMembers.hcp65project), so the files made in it have no File Header, or szLicence.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'MoveMembers')
	shutil.copytree(os.path.join(HERE, 'Input', 'MoveMembers'), szFolder)
	if szLicence:
		open(os.path.join(szFolder, 'MoveMembers.hcp65project'), 'w', newline='').write('<?xml version="1.0" encoding="utf-8"?>\n<HCP65Project Licence="%s"/>\n' % szLicence)
	WriteSchemeTemplates(szFolder, asTemplates)
	return {'SOLUTION': os.path.join(szFolder, 'MoveMembers.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeInlineVariableCopy():
	#A copy of Input/InlineVariable (Flow.cpp: functions whose variables have one value, a value that is a call, a
	#value whose variable is changed, and variables that can't be inlined), to inline variables in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'InlineVariable')
	shutil.copytree(os.path.join(HERE, 'Input', 'InlineVariable'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'InlineVariable.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeIntroduceTypedefCopy():
	#A copy of Input/IntroduceTypedef (CList, a class template whose members' types name its parameter; CItems, which
	#writes CList<SItem> and unsigned long long several times), to introduce typedefs in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'IntroduceTypedef')
	shutil.copytree(os.path.join(HERE, 'Input', 'IntroduceTypedef'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'IntroduceTypedef.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeExtractMethodCopy():
	#A copy of Input/ExtractMethod (CMix, whose functions have statements that read, change and return variables in
	#every way; CPile, a class template; Larger, a function template), to extract methods in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'ExtractMethod')
	shutil.copytree(os.path.join(HERE, 'Input', 'ExtractMethod'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'ExtractMethod.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeFindDuplicatesCopy():
	#A copy of Input/FindDuplicates (CLedger, in Ledger.cpp, and CBook, in Book.cpp, whose functions are written again
	#in each other with other names: a whole body, a run of statements, an expression and one with other literals), to
	#find duplicates in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'FindDuplicates')
	shutil.copytree(os.path.join(HERE, 'Input', 'FindDuplicates'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'FindDuplicates.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeExceptionsCopy(bExceptions=True):
	#A copy of Input/Exceptions (ExceptionsPlan.txt stage 10), a solution whose build configuration has Exceptions="true"
	#(taken out without bExceptions): CFault is thrown by Check (declared throw(CFault)) and Choose, caught by Guarded,
	#Wrapped's body is a function-try-block, Promise is noexcept and calls Check, Safe is noexcept and calls nothing.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Exceptions')
	shutil.copytree(os.path.join(HERE, 'Input', 'Exceptions'), szFolder)
	if not bExceptions:
		szFile = os.path.join(szFolder, 'Exceptions.hcp65build')
		open(szFile, 'w', newline='').write(open(szFile, newline='').read().replace(' Exceptions="true"', ''))
	return {'SOLUTION': os.path.join(szFolder, 'Exceptions.sln'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeInlineFunctionCopy():
	#A copy of Input/InlineFunction (CPump, with functions that return an expression, have statements, and can't be
	#inlined each for its own reason; CSlot, a class template; Twice and Report, functions of no class), to inline
	#functions in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'InlineFunction')
	shutil.copytree(os.path.join(HERE, 'Input', 'InlineFunction'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'InlineFunction.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeEncapsulateFieldCopy():
	#A copy of Input/EncapsulateField (CDial, whose public fields a function outside it reads, assigns, adds to,
	#steps and takes the addresses of; CKnob, which has a protected part), to encapsulate fields in.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'EncapsulateField')
	shutil.copytree(os.path.join(HERE, 'Input', 'EncapsulateField'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'EncapsulateField.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeIncludesCopy():
	#A copy of Input/Includes, to put #includes right in: CTank's header #includes a header it names nothing of, one
	#it only needs a forward declaration from and one it only uses a macro of, and the files that #include it get
	#names through those; CDrum's header #includes nothing and compiles where its source has what it needs first;
	#Flow.cpp names a class whose header it hasn't; Sink.cpp has an #include only the header after it needs.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Includes')
	shutil.copytree(os.path.join(HERE, 'Input', 'Includes'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Includes.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeMacrosCopy():
	#A copy of Input/Macros, to convert and inline macros in: Macros.h #defines values (a number, an expression of
	#another, a string, a float), a run of whole numbers, names an #if and an #ifdef use, and macros with parameters
	#(one used with ints and with floats, one whose argument does something, one without brackets, one that changes
	#its argument, one that makes a string, one that is statements, one used inside itself, one of several lines);
	#Use.cpp uses them all.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Macros')
	shutil.copytree(os.path.join(HERE, 'Input', 'Macros'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Macros.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeEnumsCopy():
	#A copy of Input/Enums, to make enums enum classes in: EMode and ELevel in Modes.h, EState in CMotor, and what
	#names their enumerators in Modes.cpp and Use.cpp (ELevel's as an integer too).
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Enums')
	shutil.copytree(os.path.join(HERE, 'Input', 'Enums'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Enums.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeTemplateInterfaceCopy():
	#A copy of Input/TemplateInterface: CRunner<M>, which calls three functions of M (one in a function defined
	#outside it, where the parameter is T), used with CDiesel; and Poke<M>, which nothing uses.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'TemplateInterface')
	shutil.copytree(os.path.join(HERE, 'Input', 'TemplateInterface'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'TemplateInterface.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeConstructorsCopy():
	#A copy of Input/Constructors: CBox, whose constructor takes two arguments, made as a variable (with ( and with
	#{), with new, as a temporary, as a base and as a member of CCrate.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Constructors')
	shutil.copytree(os.path.join(HERE, 'Input', 'Constructors'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Constructors.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakePointersCopy():
	#A copy of Input/Pointers, to make pointers references in and back: CVat's functions take SLevel by pointer and
	#by reference (one overridden, one that calls itself, one that tests its pointer, one that steps it), and Use
	#calls them with addresses, pointers and NULL and has pointer and reference locals.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Pointers')
	shutil.copytree(os.path.join(HERE, 'Input', 'Pointers'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Pointers.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeQualifiersCopy():
	#A copy of Input/Qualifiers, to make member functions const and static in: CMeter's functions read its fields,
	#assign them (one an array's element, one a struct's member), call each other, change what a pointer field
	#points to, and use nothing of the object at all; one is virtual and overridden.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Qualifiers')
	shutil.copytree(os.path.join(HERE, 'Input', 'Qualifiers'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Qualifiers.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeTemplatesCopy():
	#A copy of Input/Templates, to extract template parameters in: functions that take a CGauge by pointer (one a
	#header declares, one a class's, one virtual, one of its file alone), one that has a CGauge only in its body,
	#one that names a type of CGauge's own, and one of ints.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'Templates')
	shutil.copytree(os.path.join(HERE, 'Input', 'Templates'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'Templates.vcxproj'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


def MakeKeymapFile(szText=None):
	#Where a scripted run keeps its keys, with szText in it if it's given.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFile = os.path.join(szRoot, 'Keymap.xml')
	if szText is not None:
		open(szFile, 'w').write(szText)
	return szFile


def MakeSettingsFile(szText=None):
	#Where a scripted run keeps how the IDE was left, with szText in it if it's given.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFile = os.path.join(szRoot, 'Settings.xml')
	if szText is not None:
		open(szFile, 'w').write(szText)
	return szFile


#Each check: its name, its HCP65_IDE_* variables, the lines (regular expressions) its output must have, in order, and
#lines it mustn't have.
CHECKS = [
	('usages-rename', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usages,references,renamable'},
		[r'^Usages of IsParentheses and 1 override  3 usages$',
		 r'^  Call  3 usages$', r'^    BaseLib  3 usages$', r'^      Calculator\.cpp  3$', r'^        CCalculator  3$', r'^          367: ',
		 r'^          391: ', r'^          421: ',
		 r'^References to IsParentheses and 1 override  7 references$',
		 r'^  Definition  2 references$', r'^  Declaration  2 references$', r'^      CalcParentheses\.h  1$', r'^          38: ',
		 r'^  Call  3 references$',
		 r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\) const\] at 7 places, 1 overrides$'], [r'\[declaration\]']),
	('usages-files-only', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,references,usagegroups:c,usages'},
		[r'^References to IsParentheses and 1 override  7 references$',
		 r'^  CalcExpression\.cpp  1$', r'^    60: .*\[definition\]$', r'^  CalcExpression\.h  1$', r'^    37: .*\[declaration\]$',
		 r'^  CalcParentheses\.cpp  1$', r'^  CalcParentheses\.h  1$', r'^  Calculator\.cpp  3$', r'^    367: ', r'^    391: ', r'^    421: ',
		 r'^Usages of IsParentheses and 1 override  3 usages$', r'^  Calculator\.cpp  3$', r'^    CCalculator  3$', r'^      367: '],
		[r'BaseLib  ', r'Call  ']),
	('usages-enter', {
		#Enter in the Find Usages tree goes to the selected usage, as double clicking it does: the second of Calculator.cpp's.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:enter'},
		[r'^  Calculator\.cpp  3$', r'^    391: ', r'Calculator\.cpp \(focus\) line 391: '], []),
	('usages-jump-to-declaration', {
		#F4 in the Find Usages tree goes to the symbol's definition, as IntelliJ's goes to the declaration, whichever usage
		#is selected.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:f4'},
		[r'^    391: ', r'CalcParentheses\.cpp \(focus\) line 72: '], [r'Calculator\.cpp \(focus\)']),
	('usages-first-selected', {
		#Find Usages selects the first usage and gives the tree the keys, for enter to go to it; grouped again, the usage
		#selected stays selected.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,printselected,select:3,usagegroups:c,printselected,usagegroups:,usages,key:enter'},
		[r'^Selected: 367: ', r'^Selected: 391: ', r'Calculator\.cpp \(focus\) line 367: '], [r'^Selected: none']),
	('usages-next-occurrence', {
		#Control+Alt+Down and Up in the Find Usages tree go to the next and previous usage, each opened in its editor with
		#the tree keeping the keys; past the last nothing moves.  (The editor opened has the keys by the time the script
		#runs, so the tree is given them again.)
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,command:Navigate.NextOccurrence,edit:printselected,edit:printselection,command:Navigate.NextOccurrence,edit:printselected,'
		         'command:Navigate.NextOccurrence,printstatus,edit:printselected,command:Navigate.PreviousOccurrence,edit:printselected,edit:print'},
		[r'^Selected: 391: ', r'^Selection: \[IsParentheses\] on line 391, caret line 391$', r'^Selected: 421: ',
		 r'^Status line: No usages after this one\.$', r'^Selected: 421: ', r'^Selected: 391: ', r'Calculator\.cpp line 391: '],
		[r'not enabled', r'Calculator\.cpp \(focus\)']),
	('usages-step-from-editor', {
		#From a source editor, Control+Alt+Down steps through Find Usages, the one tool window seen with something to step
		#to, from the usage selected; the editor opened has the keys, so it goes on from there.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:editor,command:Navigate.NextOccurrence,edit:printselected,command:Navigate.NextOccurrence,'
		         'edit:printselected,edit:printselection,command:Navigate.PreviousOccurrence,edit:printselected,edit:print'},
		[r'^Selected: 391: ', r'^Selected: 421: ', r'^Selection: \[IsParentheses\] on line 421, caret line 421$', r'^Selected: 391: ',
		 r'Calculator\.cpp \(focus\) line 391: '],
		[r'not enabled', r'Nothing to step through']),
	('usages-step-behind-build', {
		#Find Usages behind the Build tab can't be seen, and Build has nothing: from the editor nothing steps.  In the IDE's
		#own layout, where they are tabbed together, whatever the user's is.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'command:Window.ResetLayout,edit:focus:build,edit:focus:editor,command:Navigate.NextOccurrence,printstatus,edit:printselected,edit:print'},
		[r'^Status line: Nothing to step through: ', r'^Selected: 367: ', r'CalcParentheses\.h \(focus\) line 38: '],
		[r'Calculator\.cpp.* line \d+: \[']),
	('build-step-from-editor', dict(MakeBrokenKernel(), **{
		#Built, the kernel's errors are stepped through from its editor: Build is the one tool window seen with something
		#to step to.  Its messages name files by their names; the kernel's is found.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:focus:editor,command:Navigate.NextOccurrence,edit:printselection,edit:print,command:Navigate.NextOccurrence,'
		         'edit:printselection,edit:print,command:Navigate.NextOccurrence,printstatus,command:Navigate.PreviousOccurrence,edit:print'}),
		[r'^Selection: \[giMissing\] on line 2, caret line 2$', r'Kernel\.cpp \(focus\) line 2: ',
		 r'^Selection: \[giAlsoMissing\] on line 5, caret line 5$', r'Kernel\.cpp \(focus\) line 5: ',
		 r'^Status line: No build messages after this one\.$', r'Kernel\.cpp \(focus\) line 2: '], [r'not enabled', r'Nothing to step through']),
	('build-step-in-pane', dict(MakeBrokenKernel(), **{
		#In the Build tab, Control+Alt+Down goes to its first message, the tab keeping the keys.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:focus:build,command:Navigate.NextOccurrence,edit:print'}),
		[r'Kernel\.cpp line 2: '], [r'not enabled', r'\(focus\)']),
	('build-message-double-click', dict(MakeBrokenKernel(), **{
		#Built, a double click on a message in the Build tab opens its place, its word selected, and the editor has the
		#keys, as IntelliJ IDEA's Build window does.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:outputdoubleclick:build:giAlsoMissing,edit:printselection,edit:print'}),
		[r'^Selection: \[giAlsoMissing\] on line 5, caret line 5$', r'Kernel\.cpp \(focus\) line 5: '], [r'not enabled']),
	('build-message-enter', dict(MakeBrokenKernel(), **{
		#Enter on a message in the Build tab opens it, and so does Jump to Source (F4).
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:outputcaret:build:giMissing],edit:key:enter,edit:print,edit:outputcaret:build:giAlsoMissing,edit:key:f4,edit:print'}),
		[r'Kernel\.cpp \(focus\) line 2: ', r'Kernel\.cpp \(focus\) line 5: '], [r'not enabled']),
	('build-message-not-a-message', dict(MakeBrokenKernel(), **{
		#A double click on a line that isn't a message (the kernel's unit's, before its messages) opens nothing: the Build
		#tab keeps the keys.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:outputdoubleclick:build:Kernel.cpp (,edit:print'}),
		[r'Kernel\.cpp line 1: '], [r'\(focus\)']),
	('build-message-step-after-open', dict(MakeBrokenKernel(), **{
		#A message opened is the current one: Next Occurrence in the Build tab goes on from it.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:outputdoubleclick:build:giMissing],edit:focus:build,command:Navigate.NextOccurrence,edit:print'}),
		[r'Kernel\.cpp line 5: '], [r'not enabled', r'\(focus\)']),
	('build-message-colours', dict(MakeBrokenKernel(), **{
		#Built, the Build tab's errors' lines are in the theme's failure colour; units' and the rest aren't coloured.  (The
		#units compile in parallel, so only the kernel's two errors, under its unit, are in a known order.)
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:printoutputcolours:build'}),
		[r'^Output colour: error \[Kernel\.cpp \(2, 22\): .*giMissing\]\]$', r'^Output colour: error \[Kernel\.cpp \(5, 15\): .*giAlsoMissing\]\]$',
		 r'^Output colours: 3$'], [r'Output colour: .*\(\d+/13\)']),
	('build-message-formats', dict(MakeBrokenKernel(), **{
		#The compilers' messages: GCC's ("file:line:column: warning:"), MSVC's ("file(line,column): error C2065:", and
		#"file(line): warning C4100:") and the HCP65 compiler's ("file (line, column): Warning:").  Warnings are in the
		#warning colour, errors in the failure colour; GCC's notes aren't coloured but can be gone to, and its other
		#lines aren't messages.  A double click on one goes to it.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'edit:outputappend:build:Kernel.cpp:5:9: warning: unused value,'
		         'edit:outputappend:build:Kernel.cpp(2;22): error C2065: giMissing: undeclared identifier,'
		         'edit:outputappend:build:Kernel.cpp(5): warning C4100: unreferenced parameter,'
		         'edit:outputappend:build:Kernel.cpp (5; 15): Warning: Only variables are supported,'
		         'edit:outputappend:build:Kernel.cpp:2:1: note: declared here,'
		         'edit:outputappend:build:In file included from Kernel.cpp:3:,'
		         'edit:outputappend:build:Kernel.cpp: In function int F(),'
		         'edit:printoutputcolours:build,edit:outputdoubleclick:build:unused value,edit:print,edit:outputdoubleclick:build:C2065,edit:print,'
		         'edit:outputdoubleclick:build:declared here,edit:printselection'}),
		[r'^Output colour: warning \[Kernel\.cpp:5:9: warning: unused value\]$', r'^Output colour: error \[Kernel\.cpp\(2,22\): error C2065: ',
		 r'^Output colour: warning \[Kernel\.cpp\(5\): warning C4100: ', r'^Output colour: warning \[Kernel\.cpp \(5, 15\): Warning: ',
		 r'^Output colours: 4$', r'Kernel\.cpp \(focus\) line 5: ', r'Kernel\.cpp \(focus\) line 2: ', r'^Selection: \[int\] on line 2, caret line 2$'],
		[r'not enabled']),
	('build-text-menu', dict(MakeBrokenKernel(), **{
		#The right button in the Build tab opens the window's text menu (it has none of its own): Cut and Paste greyed out,
		#as it can't be edited.  Select All then Copy (the right button in the selection, on its last line, which is in
		#view) puts its text on the clipboard, and the tab keeps the keys.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1',
		'DEBUG': 'wait:6000,edit:outputmenu:build:giMissing],edit:outputmenuclick:Select All,edit:outputmenu:build:build failed,'
		         'edit:outputmenuclick:Copy,edit:printclipboard,edit:print'}),
		[r'^Tab window: main$', r'^Menu Text: Cut \[[^]]*\]\(disabled\), Copy \[[^]]*\], Paste \[[^]]*\]\(disabled\), Select All \[[^]]*\]$', r'^Menu Text: Cut',
		 r'^Clipboard:$', r'^Kernel: ', r'^Kernel\.cpp \(2, 22\): .*giMissing'], [r'\(focus\)', r'Menu Text: none']),
	('build-text-menu-floated', dict(MakeBrokenKernel(), **{
		#Floated in a tool window of its own, the Build tab has the text menu too, and its Copy copies there.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1', 'DOCK': 'float:Build',
		'DEBUG': 'wait:6000,edit:outputmenu:build:build failed,edit:outputmenuclick:Select All,edit:outputmenu:build:build failed,'
		         'edit:outputmenuclick:Copy,edit:printclipboard'}),
		[r'^Tab window: floated$', r'^Menu Text: Cut \[[^]]*\]\(disabled\), Copy', r'^Menu Text: Cut', r'^Clipboard:$', r'^Kernel: '], [r'Menu Text: none', r'Tab window: main']),
	('edit-menu-floated', dict(MakeBrokenKernel(), **{
		#With the Build tab floated and its text selected by its keys, the main menu's Edit > Copy copies it there: the
		#Edit commands act on what the keys last went to in a tool window too, and the editor doesn't take the keys.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'BUILD': '1', 'DOCK': 'float:Build',
		'DEBUG': 'wait:6000,edit:outputcaret:build:build failed,edit:keys:Control+A,clickmenu:Edit/Copy,edit:printclipboard,edit:print'}),
		[r'^Clipboard:$', r'^Kernel: ', r'Kernel\.cpp line 1: '], [r'Kernel\.cpp \(focus\)']),
	('indexing-text-menu', dict(MakeIndexingProblemsKernel(), **{
		#The Indexing tab has the text menu too; the editor keeps its own menu.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:outputmenu:indexing:Defs.h,edit:outputmenuclick:Copy'}),
		[r'^Menu Text: Cut \[[^]]*\]\(disabled\), Copy \[[^]]*\], Paste \[[^]]*\]\(disabled\), Select All \[[^]]*\]$'], [r'Menu Text: none']),
	('indexing-full-paths', dict(MakeIndexingProblemsKernel(), **{
		#The Indexing tab's problems name the files they are in by their full paths (a header's name can be in more than
		#one include directory), in the failure colour.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,printindexing,edit:printoutputcolours:indexing'}),
		[r'^Indexing: \d+ ms, 1 units with problems$', r'^(/|[A-Za-z]:\\)\S*[/\\]Workspace[/\\]Kernel[/\\]Defs\.h \(2, 9\): The macro is defined again differently',
		 r'^(/|[A-Za-z]:\\)\S*[/\\]Workspace[/\\]Kernel[/\\]Kernel\.cpp \(2, 10\): Could not include file "Gone\.h"$', r'^Output colour: error \[(/|[A-Za-z]:\\)\S*[/\\]Defs\.h ',
		 r'^Output colour: error \[(/|[A-Za-z]:\\)\S*[/\\]Kernel\.cpp ', r'^Output colours: 2$'], []),
	('indexing-step-in-pane', dict(MakeIndexingProblemsKernel(), **{
		#In the Indexing tab, Next Occurrence steps through its problems, the tab keeping the keys, to the last.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:focus:indexing,command:Navigate.NextOccurrence,edit:print,command:Navigate.NextOccurrence,edit:print,'
		         'command:Navigate.NextOccurrence,printstatus'}),
		[r'Defs\.h line 2: \[#define V 2\]', r'Kernel\.cpp line 2: \[#include "Gone\.h"\]',
		 r'^Status line: No indexing problems after this one\.$'], [r'not enabled', r'\(focus\)']),
	('indexing-double-click', dict(MakeIndexingProblemsKernel(), **{
		#A double click on a problem in the Indexing tab opens the header it is in, and the editor has the keys.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:outputdoubleclick:indexing:Defs.h,edit:print'}),
		[r'Defs\.h \(focus\) line 2: \[#define V 2\]'], [r'not enabled']),
	('steps-ambiguous', dict(MakeBrokenKernel(), **{
		#Find Usages floated beside the Build tab, both seen with something to step to: from the editor neither steps.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '2:6', 'BUILD': '1', 'DOCK': 'float:FindUsages', 'EDIT': 'usages',
		'DEBUG': 'wait:6000,edit:focus:editor,command:Navigate.NextOccurrence,printstatus,edit:print'}),
		[r'^Usages of F  1 usage$', r'^Status line: Find Usages and Build can both step: click in the one to step through\.$',
		 r'Kernel\.cpp \(focus\) line 2: '], [r'Kernel\.cpp \(focus\) line 5: ']),
	('nav-back-forward', {
		#A jump (enter in Find Usages) puts where it left on Back; Back goes there and puts where it was on Forward; Forward
		#goes back again.  (The tabs the user's layout opens can be on Back before them.)
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,edit:printhistory,command:Navigate.Back,edit:printhistory,edit:print,'
		         'command:Navigate.Forward,edit:printhistory,edit:print'},
		[r'^History: back \[(.*, )?CalcParentheses\.h:38\] forward \[\]$', r'^History: back \[.*\] forward \[Calculator\.cpp:367\]$',
		 r'CalcParentheses\.h \(focus\) line 38: ', r'^History: back \[(.*, )?CalcParentheses\.h:38\] forward \[\]$',
		 r'Calculator\.cpp \(focus\) line 367: '], [r'not enabled']),
	('nav-steps', {
		#Each Next Occurrence step is a place: Back goes back through them one at a time.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:editor,command:Navigate.NextOccurrence,command:Navigate.NextOccurrence,edit:printhistory,'
		         'command:Navigate.Back,edit:print,command:Navigate.Back,edit:print'},
		[r'^History: back \[(.*, )?CalcParentheses\.h:38, Calculator\.cpp:391\] forward \[\]$', r'Calculator\.cpp \(focus\) line 391: ',
		 r'CalcParentheses\.h \(focus\) line 38: '], [r'not enabled']),
	('nav-closed-file', {
		#Back to a place whose file has been closed opens it again, its caret there.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,closeall,command:Navigate.Back,edit:print'},
		[r'CalcParentheses\.h \(focus\) line 38: '], [r'not enabled', r'Calculator\.cpp.* line \d+: \[']),
	('nav-jump-clears-forward', {
		#A new jump after Back clears Forward, as a browser's does.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,command:Navigate.Back,edit:printhistory,edit:focus:usages,edit:select:3,edit:key:enter,'
		         'edit:printhistory,command:Navigate.Forward'},
		[r'^History: back \[.*\] forward \[Calculator\.cpp:367\]$', r'^History: back \[(.*, )?CalcParentheses\.h:38\] forward \[\]$',
		 r'^Command \[Navigate\.Forward\] not found, not enabled or did nothing\.$'], []),
	('nav-edit-follows', {
		#A place in an open file moves with its line: two lines put in above it move it down two.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,edit:focus:usages,edit:select:4,edit:key:enter,edit:printhistory,'
		         'edit:focus:editor,edit:caret:1:1,edit:key:enter,edit:key:enter,edit:printhistory,command:Navigate.Back,edit:print'},
		[r'^History: back \[(.*, )?CalcParentheses\.h:38, Calculator\.cpp:367\] forward \[\]$',
		 r'^History: back \[(.*, )?CalcParentheses\.h:38, Calculator\.cpp:369\] forward \[\]$',
		 r'Calculator\.cpp \* \(focus\) line 369: \[\s*if \(\(\*ppcExpression\)->IsParentheses\(\)\)\]'], [r'not enabled']),
	('nav-far-move', {
		#The caret moved far from where it stayed (Control+End) is a place to come back to.
		'OPEN': 'Calculator.cpp', 'CARET': '10:1',
		'DEBUG': 'wait:1500,edit:focus:editor,edit:key:documentend,wait:1500,edit:printhistory,command:Navigate.Back,edit:print'},
		[r'^History: back \[(.*, )?Calculator\.cpp:10\] forward \[\]$', r'Calculator\.cpp \(focus\) line 10: '], [r'not enabled']),
	('gotosource-usages-popup', {
		#Control+B on a symbol's own declaration shows a chooser of its usages, as IntelliJ's, the first selected; Enter
		#goes to it and the chooser closes.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'DEBUG': 'gotosource,edit:printpopup,edit:key:enter,edit:printpopup,edit:print'},
		[r'^Popup: Usages of IsParentheses and 1 override  3 usages \| Calculator\.cpp:367  if \(\(\*ppcExpression\)->IsParentheses\(\)\) \| '
		 r'Calculator\.cpp:391  if \(pcExpression->IsParentheses\(\)\) \| Calculator\.cpp:421  if \(pcExpression->IsParentheses\(\)\)$',
		 r'^Popup: none$', r'Calculator\.cpp \(focus\) line 367: '], []),
	('gotosource-popup-scrolls', {
		#CCodabase's many usages: the chooser's rows are no taller than the theme's most rows, and scroll.
		'WAIT_INDEX': '1', 'OPEN': 'Codabase.h', 'CARET': '27:8',
		'DEBUG': 'gotosource,edit:printpopup'},
		[r'^Popup: Usages of CCodabase  \d\d+ usages \| ', r'^Popup height: \d+ scrolling, the rows \d+$'], [r'all shown']),
	('gotosource-on-usage', {
		#Control+B on a usage still goes to the definition (CCalcExpression's, as pcExpression is one), with no popup.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'Calculator.cpp', 'CARET': '391:22',
		'DEBUG': 'gotosource,edit:printpopup,edit:print'},
		[r'^Popup: none$', r'CalcExpression\.cpp \(focus\) line 60: '], []),
	('gotosource-after-name', dict(MakeUsagesKernel(), **{
		#Control+B with the caret just after a name, as after it is typed, goes to its definition, as Find Usages and
		#Rename find the name there.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '3:23',
		'DEBUG': 'command:Navigate.GoToSource,edit:print'}),
		[r'Kernel\.cpp \(focus\) line 1: '], [r'not enabled', r'No definition found']),
	('gotosource-after-step', dict(MakeUsagesKernel(), **{
		#A step through the usages leaves the name selected with the caret after it: Control+B goes to its definition.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:editor,command:Navigate.NextOccurrence,edit:printselection,command:Navigate.GoToSource,edit:print'}),
		[r'^Selection: \[F\] on line 3, caret line 3$', r'Kernel\.cpp \(focus\) line 1: '], [r'not enabled', r'No definition found']),
	('usages-step-tree-order', dict(MakeUsagesKernel(), **{
		#A class's lines are together under its node, so the tree's order isn't the file's: Control+Alt+Down steps down
		#the tree, from line 2 to CA's other line, 4, before lines 3 and 5.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:c,usages',
		'DEBUG': 'edit:focus:usages,command:Navigate.NextOccurrence,edit:printselected,command:Navigate.NextOccurrence,edit:printselected,'
		         'command:Navigate.NextOccurrence,edit:printselected'}),
		[r'^Usages of F  4 usages$', r'^  Kernel\.cpp  4$', r'^    CA  2$', r'^      2: class CA ', r'^      4: int CA::N', r'^    3: int G', r'^    5: int main',
		 r'^Selected: 4: ', r'^Selected: 3: ', r'^Selected: 5: '], [r'not enabled']),
	('usages-regroup-after-reindex', dict(MakeUsagesKernel(), **{
		#A usage keeps the class it was found in: grouped by class while the index is being built again, CA's are still
		#under CA.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages,reindexall,usagegroups:c,printusages'}),
		[r'^Usages of F  4 usages$', r'^  Kernel\.cpp  4$', r'^    2: ', r'^Usages of F  4 usages$', r'^  Kernel\.cpp  4$', r'^    CA  2$', r'^      2: ', r'^      4: '], []),
	('usages-same-file-names', dict(MakeUsagesKernel(USAGES_KERNEL,
		{'A/Same.cpp': 'int F(void);\nint SA(void) { return F(); }\n', 'B/Same.cpp': 'int F(void);\nint SB(void) { return F(); }\n'}), **{
		#Files are in the order of their names, and two of one name are told apart by their folders, after their counts.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages'}),
		[r'^Usages of F  6 usages$', r'^  Kernel\.cpp  4$', r'^  Same\.cpp  1  .*[/\\]A$', r'^    2: int SA', r'^  Same\.cpp  1  .*[/\\]B$', r'^    2: int SB'],
		[r'Kernel\.cpp  4  ']),
	('usages-follow-edits', dict(MakeUsagesKernel(), **{
		#A usage follows its file's edits: two lines put in at the top, and "x = " before the F on what was line 3, the
		#step to it selects F on line 5, and the tree, once the edits have stopped, says 5.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:editor,edit:caret:1:1,edit:key:enter,edit:key:enter,edit:caret:5:15,edit:type:int x = ,wait:900,edit:printusages,'
		         'command:Navigate.NextOccurrence,edit:printselection,edit:printselected'}),
		[r'^Usages of F  4 usages$', r'^    2: class CA', r'^    3: int G', r'^Usages of F  4 usages$', r'^    4: class CA',
		 r'^    5: int G\(void\) \{ int x = return F\(\); \}$', r'^    6: int CA::N', r'^    7: int main',
		 r'^Selection: \[F\] on line 5, caret line 5$', r'^Selected: 5: int G'], [r'not enabled']),
	('gotosource-popup-press-outside', dict(MakeUsagesKernel(), **{
		#A press outside Control+B's popup closes it, in the editor too, which is its owner.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5',
		'DEBUG': 'gotosource,edit:printpopup,edit:presseditor,edit:printpopup,edit:print'}),
		[r'^Popup: Usages of F  4 usages \| ', r'^Pressed: the editor$', r'^Popup: none$', r'Kernel\.cpp.* line 1: '], []),
	('usages-include-in-folder', dict(MakeUsagesKernel(
		'int F(void);\n#include "A/Inner.h"\nint F(void) { return SA(); }\n', {'A/Inner.h': 'inline int SA(void) { return F(); }\n'}), **{
		#A header included by a path with a folder, from beside the file that includes it, is indexed: F's usage in it is
		#found, and Control+B on SA goes to it there.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:caret:3:22,command:Navigate.GoToSource,edit:print,printindexing'}),
		[r'^Usages of F  1 usage$', r'^  Inner\.h  1$', r'^    1: inline int SA', r'Inner\.h \(focus\) line 1: ', r'^Indexing: \d+ ms, 0 units with problems$'],
		[r'not enabled', r'No definition found']),
	('usages-groups-kept', dict(MakeUsagesKernel(), **{
		#Find Usages' groups are kept when they change, for the next time the IDE is run.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'SETTINGS': MakeSettingsFile(), 'EDIT': 'usagegroups:c,usages',
		'DEBUG': 'printsettings'}),
		[r'^  Kernel\.cpp  4$', r'^    CA  2$', r'^Settings file:$', r'<FindUsages GroupByType="false" GroupByProject="false" GroupByClass="true"'], []),
	('usages-groups-read', dict(MakeUsagesKernel(), **{
		#Those kept come back as the IDE starts: by usage type alone here, and the one the file doesn't say stays on.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usages',
		'SETTINGS': MakeSettingsFile('<Settings>\n  <FindUsages GroupByProject="false" GroupByClass="false"/>\n</Settings>\n')}),
		[r'^Usages of F  4 usages$', r'^  Call  4 usages$', r'^    Kernel\.cpp  4$', r'^      2: class CA'], [r'^\s+Kernel  4 usages$', r'^\s+CA  2$']),
	('usages-many-files-open', dict(MakeUsagesKernel(USAGES_KERNEL, {'Aaa.cpp': ManyUsages('A', 600), 'Bbb.cpp': ManyUsages('B', 600)}), **{
		#However many usages there are (1204 here) the tree's files start open: the top, the three files and every line
		#can be seen (the tree only has components for the rows in view).  A step from Aaa.cpp's last line goes to
		#Bbb.cpp's first.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages,printrows,printselected,select:601',
		'DEBUG': 'edit:focus:usages,command:Navigate.NextOccurrence,edit:printselected,edit:printrows'}),
		[r'^Usages of F  1204 usages$', r'^  Aaa\.cpp  600$', r'^  Bbb\.cpp  600$', r'^  Kernel\.cpp  4$', r'^Rows: 1208$',
		 r'^Selected: 2: int A0\(void\)', r'^Selected: 2: int B0\(void\)', r'^Rows: 1208$'], [r'not enabled']),
	('usages-thousands-open', dict(MakeUsagesKernel(USAGES_KERNEL, {'Big.cpp': ManyUsages('A', 20000)}), **{
		#20,004 usages, 20,000 of them in one file: closed and opened again, laid out, painted and scrolled to the
		#end, each in under a second (timeusagetree).  Before the tree only had components for the rows in view,
		#opening them took 4.8 seconds.  Closing them left the top node selected, the open node the selected line was in.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages,printrows,timeusagetree,printselected'}),
		[r'^Usages of F  20004 usages$', r'^  Big\.cpp  20000$', r'^Rows: 20007$',
		 r'^Usage tree: 20007 rows, open \d{1,3} ms, layout \d{1,3} ms, paint \d{1,3} ms, scroll \d{1,3} ms$', r'^Selected: Usages of F  20004 usages$'], []),
	('gotosource-popup-more', dict(MakeUsagesKernel(USAGES_KERNEL, {'Aaa.cpp': ManyUsages('A', 150)}), **{
		#Control+B's chooser lists every usage, up to a thousand; its Open in Find Window button closes it and lists
		#them all in Find Usages.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5',
		'DEBUG': 'gotosource,edit:printpopup,edit:chooserbutton,edit:printpopup,edit:printusages'}),
		[r'^Popup: Usages of F  154 usages \| Aaa\.cpp:2  int A0\(void\).* \| Aaa\.cpp:151  int A149\(void\) \{ return F\(\); \}',
		 r'^Popup height: \d+ scrolling, the rows \d+$', r'^Popup: none$', r'^Usages of F  154 usages$'], [r'and 54 more']),
	('usages-keys', {
		#Find Usages' keys, and Jump to Source's, are bound: F4, Alt+F7 and Control+Alt+Down and Up.
		'DEBUG': 'printkeys'},
		[r'^Keys: Window\.JumpToSource F4 bound$', r'^Keys: Navigate\.FindUsages Alt\+F7 bound$',
		 r'^Keys: Navigate\.NextOccurrence Control\+Alt\+Cursor Down bound$', r'^Keys: Navigate\.PreviousOccurrence Control\+Alt\+Cursor Up bound$',
		 r'^Keys: Navigate\.Back Control\+Alt\+Cursor Left bound$', r'^Keys: Navigate\.Forward Control\+Alt\+Cursor Right bound$'],
		[r'NOT BOUND']),
	('usages-typing', {
		#Space in the Find Usages tree does nothing: it doesn't go to the usage as enter does (the user, 2026-11-12).
		#A character typed finds a row of the tree (nav-type-to-find-usages); neither is given to the editor, whose file
		#they were once typed into: the editor doesn't get the keys and its line is as it was.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:space,key:type:ab'},
		[r'^    391: ', r'CalcParentheses\.h line 38: \[\tbool\t\tIsParentheses\(void\) const override;\]$'],
		[r'Calculator\.cpp.* line \d+: \[', r'CalcParentheses\.h.*\(focus\)', r'CalcParentheses\.h \*', r' ab\]']),
	('usages-single-click', {
		#With Navigate with Single Click a usage is opened as it is selected, and the tree keeps the keys; without, selecting
		#one opens nothing.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,focus:usages,select:3,print,usagesingleclick:on,select:4,print'},
		[r'CalcParentheses\.h line 38: ', r'Calculator\.cpp line 421: '], [r'line 391: \[', r'Calculator\.cpp \(focus\)']),
	('find-jump-to-source', {
		#The Find tool window's results are of nothing declared: F4 opens the selected one, as enter does.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,edit:key:f4,edit:print'},
		[r'CalculatorParser\.h \(focus\) line 29: '], [r'ERROR']),
	('usages-escape', {
		#Escape in the Find Usages tree gives the keys back to the editor, where they were.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:escape'},
		[r'^    391: ', r'CalcParentheses\.h \(focus\) line 38: '], [r'Calculator\.cpp.* line \d+: \[']),
	('rename-after-edits', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2',
		'EDIT': 'type:int i; ,rename:mpcCalc2,rename:mpcCalc3,renamable'},
		[r'^Status: Renamed mpcCalculator to mpcCalc2: 19 places in 2 files\.$',
		 r'^Status: Renamed mpcCalc2 to mpcCalc3: 19 places in 2 files\.$',
		 r'^Rename: mpcCalc3 \[CCalculatorParser::mpcCalc3\] at 19 places, 0 overrides$'], [r'Rename stopped']),
	('rename-class-twice', {
		#A class renamed (its header, its constructor and destructor, files opened as hidden tabs) and renamed again: the
		#second rename finds every place by the new name, as the renamed files are indexed again before it looks.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '33:8',
		'EDIT': 'rename:CCalcParser2,rename:CCalcParser3,renamable'},
		[r'^Status: Renamed CCalculatorParser to CCalcParser2: 12 places in 2 files\.$',
		 r'^Status: Renamed CCalcParser2 to CCalcParser3: 12 places in 2 files\.$',
		 r'^Rename: CCalcParser3 \[CCalcParser3\] at 12 places, 0 overrides$'], [r'Rename stopped']),
	('rename-search-type', {
		#A class renamed with Rename's searches: in comments and strings (Shape.h's comment, Shape.cpp's comment twice and its
		#string), and as text the index doesn't have (Shape_Linux.cpp, which LINUX_GNU leaves out of Debug|Win32, and
		#Notes.txt, but not its CShapes).
		'SOLUTION': RENAME_SEARCH, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '13:8', 'EDIT': 'renamesearch:ct,rename:CFigure'},
		[r'^Status: Renamed CShape to CFigure: 4 places, 2 text occurrences, 4 in comments and strings in 4 files\.$'], [r'Rename stopped']),
	('rename-search-off', {
		#Without the searches only the places the index has are renamed.
		'SOLUTION': RENAME_SEARCH, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '13:8', 'EDIT': 'renamesearch:--,rename:CFigure'},
		[r'^Status: Renamed CShape to CFigure: 4 places in 2 files\.$'], [r'text occurrence', r'in comments']),
	('rename-search-member', {
		#A member's text search finds both Draws in the code LINUX_GNU leaves out, but not CPen::Draw's declaration,
		#definition or call in the code the index has.
		'SOLUTION': RENAME_SEARCH, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '19:8', 'EDIT': 'renamesearch:ct,rename:Paint'},
		[r'^Status: Renamed Draw to Paint: 3 places, 2 text occurrences in 3 files\.$'], [r'Rename stopped']),
	('rename-file-includes', {
		#The #includes that find Engine.h (Rename File step 2): relative, with a backslash, indented "# include", <> through
		#the include path, in code LINUX_GNU leaves out, and from the Tool project; not the ones in comments, not
		#EngineCache.h's, and not Other/OtherUser.cpp's, which finds Other/Engine.h.
		'SOLUTION': RENAME_FILE, 'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'EDIT': 'printincludes'},
		[r'^Includes of Engine\.h: 7$', r'^  Commented\.cpp 6:11 8$', r'^  Engine\.cpp 1:11 8$', r'^  Engine_Linux\.cpp 2:11 8$',
		 r'^  Sub[/\\]Angle\.cpp 1:11 8$', r'^  Sub[/\\]Indented\.cpp 1:16 8$', r'^  Sub[/\\]User\.cpp 1:14 8$', r'^  Tool[/\\]Tool\.cpp 1:14 8$'],
		[r'OtherUser', r'Commented\.cpp [13]:']),
	('rename-file-related', dict(MakeRenameFileCopy(), **{
		#Rename File (step 3) on Engine.h with its related files, Engine.cpp and Engine_Linux.cpp (not EngineCache.h): the
		#files and both projects' items renamed, the 7 #includes changed and saved, Engine.h's tab following it.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h',
		'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:printincludes,printprojectfiles,printtabs,printdiskfile:Sub/User.cpp,'
		         'printdiskfile:Tool/Tool.vcxproj,printdiskfile:CPPEngine_Linux.cpp,printdiskfile:Commented.cpp'}),
		[r'^Status: Renamed Engine\.h to CPPEngine\.h and 2 more files: 7 #includes in 7 files, 2 projects\.$',
		 r'^Includes of CPPEngine\.h: 7$', r'^  Sub[/\\]Indented\.cpp 1:16 11$',
		 r'^Project files: Commented\.cpp CPPEngine\.cpp CPPEngine_Linux\.cpp Other[/\\]OtherUser\.cpp Sub[/\\]Angle\.cpp Sub[/\\]Indented\.cpp Sub[/\\]User\.cpp '
		 r'CPPEngine\.h EngineCache\.h Other[/\\]Engine\.h Tool[/\\]Tool\.cpp$',
		 r'^Tabs: >CPPEngine\.h ', r'^#include "\.\./CPPEngine\.h"', r'<ClInclude Include="\.\.\\CPPEngine\.h" />', r'^#include "CPPEngine\.h"',
		 r'^//#include "Engine\.h"$'],
		[r'Rename stopped', r"Can't rename", r'Project files:.* Engine\.h', r'Disk file .*:\n\(none\)']),
	('rename-file-one', dict(MakeRenameFileCopy(), **{
		#Rename File on Engine.h alone: Engine.cpp keeps its name, and its #include is changed.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'DEBUG': 'edit:renamefile:Motor.h,printprojectfiles,printdiskfile:Engine.cpp'}),
		[r'^Status: Renamed Engine\.h to Motor\.h: 7 #includes in 7 files, 2 projects\.$',
		 r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp .* Motor\.h EngineCache\.h', r'^#include "Motor\.h"$'],
		[r'Rename stopped', r'more file']),
	('rename-file-undo', dict(MakeRenameFileCopy(), **{
		#Undo Across Files undoes all of Rename File (step 4): the files renamed back on the disk, in the projects and in
		#Engine.h's tab, the #includes as they were, saved.  The first press shows a file it changed; the second undoes.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h',
		'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:undoall,edit:undoall,printprojectfiles,printtabs,printdiskfile:Sub/User.cpp,printdiskfile:Engine.cpp,'
		         'printdiskfile:Tool/Tool.vcxproj'}),
		[r'^Status: Renamed Engine\.h to CPPEngine\.h and 2 more files: 7 #includes in 7 files, 2 projects\.$',
		 r'^Status: Ctrl\+Z again to undo the change in Commented\.cpp at line 1\.$',
		 r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp Other[/\\]OtherUser\.cpp Sub[/\\]Angle\.cpp Sub[/\\]Indented\.cpp Sub[/\\]User\.cpp '
		 r'Engine\.h EngineCache\.h Other[/\\]Engine\.h Tool[/\\]Tool\.cpp$',
		 r'^Tabs: Engine\.h ', r'^#include "\.\./Engine\.h"', r'^#include "Engine\.h"', r'<ClInclude Include="\.\.\\Engine\.h" />'],
		[r"Couldn't rename", r'CPPEngine\.h"', r'Disk file .*:\n\(none\)']),
	('rename-file-redo', dict(MakeRenameFileCopy(), **{
		#Redo Across Files after the undo renames them all again (each pressed twice, the first showing a file changed if
		#it isn't the one active).
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h',
		'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:undoall,edit:undoall,printprojectfiles,edit:redoall,edit:redoall,printprojectfiles,printtabs,'
		         'printdiskfile:Sub/User.cpp,printdiskfile:CPPEngine.cpp'}),
		[r'^Project files: Commented\.cpp Engine\.cpp ', r'^Project files: Commented\.cpp CPPEngine\.cpp CPPEngine_Linux\.cpp .* CPPEngine\.h EngineCache\.h',
		 r'^Tabs: CPPEngine\.h ', r'^#include "\.\./CPPEngine\.h"', r'^#include "CPPEngine\.h"'],
		[r"Couldn't rename", r'Disk file .*:\n\(none\)']),
	('rename-file-undo-in-file', dict(MakeRenameFileCopy(), **{
		#Control+Z in one of the files Rename File edited undoes the whole of it, the renames too.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:open:Sub/User.cpp,edit:undo,printprojectfiles,printdiskfile:Tool/Tool.cpp'}),
		[r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp .* Engine\.h EngineCache\.h', r'^#include "\.\./Engine\.h"'],
		[r"Couldn't rename", r'Project files:.*CPPEngine', r'CPPEngine\.h"']),
	('rename-file-dialog', dict(MakeRenameFileCopy(), **{
		#Rename File's dialog (step 5), from the editors' Refactor menu, with its ticks as they start: the stem typed over,
		#the related files renamed too, and Engine.h in Commented.cpp's comment, but not on #include lines that aren't
		#Engine.h's (Other/OtherUser.cpp's) or in a comment (Commented.cpp's /* */).
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:CPPEngine', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'printmenus,command:Refactor.RenameFile,answered:1,printprojectfiles,printdiskfile:Commented.cpp,printdiskfile:Other/OtherUser.cpp'}),
		[r'^Menu Editor/Refactor: Rename File\.\.\., .*Extract Method', r'^Menu Explorer: .*, Rename\.\.\.(\(disabled\))?, Properties\.\.\.(\(disabled\))?, Always Select Opened File, Open Files with Single Click$',
		 r'^Dialog \[Rename File\] answered OK$',
		 r'^Status: Renamed Engine\.h to CPPEngine\.h and 2 more files: 7 #includes, 1 in comments and strings in 7 files, 2 projects\.$',
		 r'^Project files: Commented\.cpp CPPEngine\.cpp CPPEngine_Linux\.cpp ', r'^//#include "CPPEngine\.h"$', r'^#include "Engine\.h"$'],
		[r'Rename stopped', r"Can't rename"]),
	('rename-file-dialog-cancel', dict(MakeRenameFileCopy(), **{
		#Cancel renames nothing.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:CPPEngine', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'command:Refactor.RenameFile,answered:1,printprojectfiles'}),
		[r'^Dialog \[Rename File\] answered Cancel$', r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp '],
		[r'^Status: Renamed']),
	('rename-file-tab-menu', dict(MakeRenameFileCopy(), **{
		#The document tab menu's Rename File renames the file of the tab it was opened for.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:Motor', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'tabmenu,command:Document.RenameFile,answered:1,printprojectfiles'}),
		[r'^Status: Renamed Engine\.h to Motor\.h and 2 more files: ', r'^Project files: Commented\.cpp Motor\.cpp Motor_Linux\.cpp '],
		[r'Rename stopped']),
	('rename-file-explorer', dict(MakeRenameFileCopy(), **{
		#Shift+F6 (Rename) with Solution Explorer's keys renames its selected file, as IntelliJ's Project view does.
		'WAIT_INDEX': '1', 'OPEN': 'Sub/User.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:Customer', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:explorer,edit:focus:explorer,command:Refactor.Rename,answered:1,printprojectfiles'}),
		[r'^Dialog \[Rename File\] answered OK$', r'^Status: Renamed User\.cpp to Customer\.cpp: no file refers to it, 1 project\.$',
		 r'^Project files: .* Sub[/\\]Customer\.cpp '],
		[r'Rename stopped', r'Sub/User\.cpp']),
	('rename-file-dialog-taken', dict(MakeRenameFileCopy(), **{
		#A name that is taken, typed in the dialog: nothing is renamed, and the status line says why.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:EngineCache', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'command:Refactor.RenameFile,answered:1,printprojectfiles'}),
		[r"^Status: Can't rename: EngineCache\.h is already there\.$", r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp '],
		[r'^Status: Renamed']),
	('rename-class-files', dict(MakeRenameFileCopy(), **{
		#A class's Rename with its files (step 6): CEngine to CMotor, Engine.h, Engine.cpp and Engine_Linux.cpp to Motor.h,
		#Motor.cpp and Motor_Linux.cpp, the #includes and both projects with them, every file edited saved.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:ctf,edit:rename:CMotor,printprojectfiles,printtabs,printdiskfile:Motor.h,printdiskfile:Motor_Linux.cpp,printdiskfile:Tool/Tool.cpp'}),
		[r'^Status: Renamed CEngine to CMotor: .*  Renamed Engine\.h to Motor\.h and 2 more files: 7 #includes, ',
		 r'^Project files: Commented\.cpp Motor\.cpp Motor_Linux\.cpp Other[/\\]OtherUser\.cpp .* Motor\.h EngineCache\.h Other[/\\]Engine\.h Tool[/\\]Tool\.cpp$',
		 r'^Tabs: >Motor\.h ', r'^class CMotor$', r'^void CMotor::InitPlatform\(void\)$', r'^#include "\.\./Motor\.h"$', r'^\tCMotor\tcEngine;$'],
		[r'Rename stopped', r"Can't rename", r'Disk file .*:\n\(none\)']),
	('rename-class-files-undo', dict(MakeRenameFileCopy(), **{
		#Undo Across Files undoes all of it: the class's name and its files.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:ctf,edit:rename:CMotor,edit:undoall,printprojectfiles,printdiskfile:Engine.h,printdiskfile:Tool/Tool.cpp'}),
		[r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp .* Engine\.h EngineCache\.h', r'^class CEngine$', r'^#include "\.\./Engine\.h"$',
		 r'^\tCEngine\tcEngine;$'],
		[r"Couldn't rename", r'Disk file .*:\n\(none\)']),
	('rename-class-no-files', dict(MakeRenameFileCopy(), **{
		#Without the f the files keep their names; a new name without the prefix letter leaves them too.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '5:8', 'DEBUG': 'edit:renamesearch:ct,edit:rename:CMotor,printprojectfiles'}),
		[r'^Status: Renamed CEngine to CMotor: ', r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp '],
		[r'Renamed Engine\.h']),
	('rename-class-files-dialog', dict(MakeRenameFileCopy(), **{
		#The Rename dialog of a class with files has Rename files, ticked as it starts, listing them.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '5:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:CMotor', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'rename,answered:1,printprojectfiles'}),
		[r'^Dialog \[Rename\] answered OK$', r'^Status: Renamed CEngine to CMotor: .*  Renamed Engine\.h to Motor\.h and 2 more files: ',
		 r'^Project files: Commented\.cpp Motor\.cpp Motor_Linux\.cpp '],
		[r'Rename stopped', r"Can't rename"]),
	('rename-class-files-digit', dict(MakeRenameDigitCopy(), **{
		#A class whose stem starts with a digit (C65Core's is 65Core) has files too: C65Core to C65C64Core renames 65Core.h
		#and 65Core.cpp to 65C64Core.h and 65C64Core.cpp, not C65CoreCycles or its 65CoreCycles.h.
		'WAIT_INDEX': '1', 'OPEN': '65Core.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:ctf,edit:rename:C65C64Core,printprojectfiles,printdiskfile:65CoreCycles.h,printdiskfile:Main.cpp'}),
		[r'^Status: Renamed C65Core to C65C64Core: .*  Renamed 65Core\.h to 65C64Core\.h and 1 more file: ',
		 r'^Project files: 65C64Core\.cpp Main\.cpp 65C64Core\.h 65CoreCycles\.h$', r'^#include "65C64Core\.h"$', r'^\tC65C64Core\*\tmpcCore;$',
		 r'^\tC65C64Core\t\t\tcCore;$', r'^\tC65CoreCycles\tcCycles;$'],
		[r'Rename stopped', r"Can't rename", r'C65C64CoreCycles', r'65C64CoreCycles']),
	('rename-file-then-class', dict(MakeRenameDigitCopy(), **{
		#After C65Core's files are renamed the projects are made again, so the index finds 65C64Core.h (it couldn't
		#include it, its header directory's list having the old name), and C65CoreCycles, whose header includes it, is
		#renamed with its files straight after.
		'WAIT_INDEX': '1', 'OPEN': '65Core.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:ctf,edit:rename:C65C64Core,waitindex,wait:1500,printindexing,edit:open:65CoreCycles.h,edit:caret:6:8,'
		         'edit:rename:C65C64CoreCycles,waitindex,wait:1500,printindexing,printprojectfiles'}),
		[r'^Indexing: \d+ ms, 0 units with problems$', r'^Status: Renamed C65CoreCycles to C65C64CoreCycles: .*  Renamed 65CoreCycles\.h to 65C64CoreCycles\.h',
		 r'^Indexing: \d+ ms, 0 units with problems$', r'^Project files: 65C64Core\.cpp Main\.cpp 65C64Core\.h 65C64CoreCycles\.h$'],
		[r'Could not include', r"can't be renamed", r'Rename stopped']),
	('rename-unsaved-warning', dict(MakeRenameDigitCopy(), **{
		#A scripted rename of a symbol alone leaves its edits unsaved, and the run says so as it ends.
		'WAIT_INDEX': '1', 'OPEN': '65Core.h', 'CARET': '5:8', 'DEBUG': 'edit:renamesearch:ct,edit:rename:C65Kernel,printdiskfile:Main.cpp'}),
		[r'^Status: Renamed C65Core to C65Kernel: ', r'^\tC65Core\t\t\tcCore;$', r'^Unsaved at exit: \d+ files?: .*65Core\.h'], []),
	('rename-saves', dict(MakeRenameDigitCopy(), **{
		#With renamesearch's s each rename: is saved: it is on the disk, and nothing is left unsaved.
		'WAIT_INDEX': '1', 'OPEN': '65Core.h', 'CARET': '5:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:C65Kernel,printdiskfile:Main.cpp'}),
		[r'^Status: Renamed C65Core to C65Kernel: ', r'^\tC65Kernel\t\t\tcCore;$'], [r'Unsaved at exit']),
	('rename-template-family', dict(MakeRenameTemplateCopy(), **{
		#A member a template's body names is renamed with its family (Refactorings.txt, T2): CHolder<M>'s mp->Init() is
		#CFoo::Init in CHolder<CFoo> and CBar::Init in CHolder<CBar>, so CFoo::Init to Begin renames CBar::Init, its
		#definition, its calls and its assembly label too, and the template's one call once.  CLone::Init, which no
		#instance names, keeps its name; so does the miCount the template reads.
		'WAIT_INDEX': '1', 'OPEN': 'Foo.h', 'CARET': '10:8',
		'DEBUG': 'edit:renamable,edit:renamesearch:ct-s,edit:rename:Begin,printdiskfile:Holder.h,printdiskfile:Foo.h,printdiskfile:Bar.h,printdiskfile:Bar.cpp,'
		         'printdiskfile:Lone.h,printdiskfile:Main.cpp,printdiskfile:HolderW65C816.asm'}),
		[r'^Rename: Init \[CFoo::Init\(\)\] at 8 places, 0 overrides$', r'^Rename family: CBar::Init in CHolder::Start$',
		 r'^Status: Renamed Init to Begin: 7 places, 4 in assembly in 7 files\.  CHolder::Start names it and CBar::Init by one name: renamed together\.$',
		 r'^\tvoid\tStart\(void\) \{ mp->Begin\(\); \}$', r'^\tint\t\tTotal\(void\) \{ return mp->miCount; \}$',
		 r'^Disk file Foo\.h:$', r'^\tvoid\tBegin\(void\);$', r'^Disk file Bar\.h:$', r'^\tvoid\tBegin\(void\);$', r'^void CBar::Begin\(void\)$',
		 r'^Disk file Lone\.h:$', r'^\tvoid\tInit\(void\);$',
		 r'^\tcFoo\.Begin\(\);$', r'^\tcBar\.Begin\(\);$', r'^\tcLone\.Init\(\);$',
		 r'^_CFoo__Begin:$', r'^_CBar__Begin:$', r'^_CLone__Init:$', r'^\t\tjsr _CFoo__Begin$', r'^\t\tjsr _CBar__Begin$', r'^\t\tjsr _CLone__Init$'],
		[r'Rename stopped', r"Can't rename", r'CLone::Begin', r'_CLone__Begin']),
	('rename-template-family-from-other', dict(MakeRenameTemplateCopy(), **{
		#From any of the family, and from the template's own call: the same members are renamed.
		'WAIT_INDEX': '1', 'OPEN': 'Bar.cpp', 'CARET': '4:12',
		'DEBUG': 'edit:renamable,edit:open:Holder.h,edit:caret:11:27,edit:renamable,edit:renamesearch:ct-s,edit:rename:Begin,printdiskfile:Foo.h,printdiskfile:Bar.h'}),
		[r'^Rename: Init \[CBar::Init\(\)\] at 8 places, 0 overrides$', r'^Rename family: CFoo::Init in CHolder::Start$',
		 r'^Rename: Init \[C(Foo|Bar)::Init\(\)\] at 8 places, 0 overrides$', r'^Rename family: C(Foo|Bar)::Init in CHolder::Start$',
		 r'^Status: Renamed Init to Begin: 7 places, ', r'^Disk file Foo\.h:$', r'^\tvoid\tBegin\(void\);$', r'^Disk file Bar\.h:$', r'^\tvoid\tBegin\(void\);$'],
		[r'Rename stopped', r"Can't rename"]),
	('rename-template-family-field', dict(MakeRenameTemplateCopy(), **{
		#A field a template's body reads is renamed with the other instances' fields of its name, not CLone's.
		'WAIT_INDEX': '1', 'OPEN': 'Foo.h', 'CARET': '8:8',
		'DEBUG': 'edit:renamable,edit:renamesearch:ct-s,edit:rename:miTotal,printdiskfile:Holder.h,printdiskfile:Bar.cpp,printdiskfile:Lone.cpp'}),
		[r'^Rename family: CBar::miCount in CHolder::Total$', r'^Status: Renamed miCount to miTotal: .*CHolder::Total names it and CBar::miCount by one name: renamed together\.$',
		 r'^\tint\t\tTotal\(void\) \{ return mp->miTotal; \}$', r'^\tmiTotal = 1;$', r'^\tmiCount = 2;$'],
		[r'Rename stopped', r"Can't rename"]),
	('rename-template-family-dialog', dict(MakeRenameTemplateCopy(), **{
		#The Rename dialog says what is renamed with it and why.
		'WAIT_INDEX': '1', 'OPEN': 'Foo.h', 'CARET': '10:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:Begin', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'rename,answered:1,edit:open:Bar.h,edit:caret:10:8,edit:print'}),
		[r'^Dialog \[Rename\] answered OK$', r'^Status: Renamed Init to Begin: .*CHolder::Start names it and CBar::Init by one name: renamed together\.$',
		 r'Bar\.h \* \(focus\) line 10: \[\tvoid\tBegin\(void\);\]$'],
		[r'Rename stopped', r"Can't rename"]),
	('rename-no-family', dict(MakeRenameTemplateCopy(), **{
		#A member no template names (CLone::Init) is renamed alone, and nothing is said of a family.
		'WAIT_INDEX': '1', 'OPEN': 'Lone.h', 'CARET': '10:8', 'DEBUG': 'edit:renamable,edit:renamesearch:ct-s,edit:rename:Begin,printdiskfile:Foo.h,printdiskfile:Holder.h'}),
		[r'^Rename: Init \[CLone::Init\(\)\] at 3 places, 0 overrides$', r'^Status: Renamed Init to Begin: 3 places, 2 in assembly in 4 files\.$',
		 r'^\tvoid\tInit\(void\);$', r'^\tvoid\tStart\(void\) \{ mp->Init\(\); \}$'],
		[r'Rename family', r'by one name']),
	('safe-delete-function', dict(MakeSafeDeleteCopy(), **{
		#Safe Delete (Refactorings.txt, 1) of a member function nothing calls: its declaration's line in the class and its
		#definition with the comment over it are taken out, the functions around it left two blank lines apart as they
		#were.  Its parameter and its local go with it.  printdisklines numbers the lines, so what is left is exact.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '26:9',
		'DEBUG': 'edit:renamesearch:---s,edit:safedelete,printdisklines:Shape.h,printdisklines:Shape.cpp'}),
		[r'^Safe Delete: CShape::Unused \[CShape::Unused\(int\)\], 4 symbols, no usages$', r'^  takes out Shape\.h 26:1-27:1$', r'^  takes out Shape\.cpp 38:1-52:1$',
		 r'^Status: Deleted CShape::Unused: 2 declarations and definitions in 2 files, with 3 things declared in it\.$',
		 r'^25\|\tint\t\tArea\(void\);$', r'^26\|\tvirtual void\tDraw\(void\);$', r'^27\|\};$', r'^54 lines$',
		 r'^35\|\}$', r'^36\|$', r'^37\|$', r'^38\|/+$', r'^39\|//$', r'^40\|//$', r'^41\|/+$', r'^42\|void CShape::Draw\(void\)$'],
		[r'\|.*Unused\(int', r'Safe Delete stopped', r'not deleted']),
	('safe-delete-used', dict(MakeSafeDeleteCopy(), **{
		#What is used isn't deleted: the usages are listed, and the files are as they were.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '23:9', 'DEBUG': 'edit:renamesearch:---s,edit:safedelete,printdisklines:Shape.h'}),
		[r'^Safe Delete: CShape::Init \[CShape::Init\(\)\], 1 symbol, 2 usages$', r'^  used at Main\.cpp 12:9$', r'^  used at Main\.cpp 16:11$',
		 r'^Safe Delete: not deleted, it is used$', r'^23\|\tvoid\tInit\(void\);$', r'^55 lines$'],
		[r'^Status: Deleted']),
	('safe-delete-anyway', dict(MakeSafeDeleteCopy(), **{
		#Deleted anyway, the status line says what is left using it.  One declarator of two goes with its comma.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '20:17', 'DEBUG': 'edit:renamesearch:---s,edit:safedelete:anyway,printdisklines:Shape.h'}),
		[r'^Safe Delete: CShape::miHeight \[CShape::miHeight\], 1 symbol, 1 usage$', r'^  takes out Shape\.h 20:14-20:24$', r'^  used at Shape\.cpp 14:2$',
		 r'^Status: Deleted CShape::miHeight: 1 declaration in 1 file\.  Left using it: 1 usage\.$', r'^20\|\tint\t\tmiWidth;$', r'^55 lines$'],
		[r'miHeight;']),
	('safe-delete-class', dict(MakeSafeDeleteCopy(), **{
		#A class nothing else uses goes with its members' definitions (their uses of each other aren't usages) and the
		#comment over it.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '39:8', 'DEBUG': 'edit:renamesearch:---s,edit:safedelete,printdisklines:Shape.h,printdisklines:Shape.cpp'}),
		[r'^Safe Delete: CLonely \[CLonely\], 4 symbols, no usages$', r'^  takes out Shape\.h 38:1-49:1$',
		 r'^Status: Deleted CLonely: 3 declarations and definitions in 2 files, with 3 things declared in it\.$',
		 r'^35\|\};$', r'^36\|$', r'^37\|$', r'^38\|typedef int\tTSpare;$', r'^44 lines$',
		 r'^67\|\}$', r'^68\|$', r'^69\|$', r'^70\|/+$', r'^75\|int UnusedFunction\(int iValue\)$'],
		[r'\|.*CLonely', r'\|.*Nothing uses it but itself', r'not deleted']),
	('safe-delete-files', dict(MakeSafeDeleteCopy(), **{
		#A class whose files have nothing else: they are deleted with it, off the disk and out of the project, and
		#the #include of its header goes from the file that has it.  Undo makes them again as they were and puts the
		#include back; redo deletes them again.
		'WAIT_INDEX': '1', 'OPEN': 'Lone.h', 'CARET': '6:8',
		'DEBUG': 'edit:renamesearch:---s,edit:safedelete,printprojectfiles,printdisklines:Lone.h,printdisklines:Lone.cpp,printdisklines:LoneUser.cpp,edit:open:LoneUser.cpp,edit:undoall,edit:undoall,printprojectfiles,'
				 'printdisklines:Lone.h,printdisklines:LoneUser.cpp,edit:redoall,edit:redoall,printprojectfiles,printdisklines:Lone.cpp,printdisklines:LoneUser.cpp'}),
		[r'^Safe Delete: CLone \[CLone\], 4 symbols, no usages$', r'^  leaves nothing in Lone\.h$', r'^  leaves nothing in Lone\.cpp$',
		 r'^Status: Deleted CLone and Lone\.h and Lone\.cpp, which had nothing else, with 3 things declared in it\.$',
		 r'^Project files: Extra\.cpp LoneUser\.cpp Main\.cpp Shape\.cpp Extra\.h Shape\.h ShapeW65C816\.asm$',
		 r'^Disk lines Lone\.h:$', r'^\(none\)$', r'^Disk lines Lone\.cpp:$', r'^\(none\)$', r'^1\|#include "Shape\.h"$', r'^2\|$', r'^11 lines$',
		 r'^Project files: Extra\.cpp Lone\.cpp LoneUser\.cpp Main\.cpp Shape\.cpp Extra\.h Lone\.h Shape\.h ShapeW65C816\.asm$',
		 r'^6\|class CLone$', r'^16 lines$', r'^1\|#include "Shape\.h"$', r'^2\|#include "Lone\.h"$', r'^12 lines$',
		 r'^Project files: Extra\.cpp LoneUser\.cpp Main\.cpp Shape\.cpp Extra\.h Shape\.h ShapeW65C816\.asm$',
		 r'^Disk lines Lone\.cpp:$', r'^\(none\)$', r'^11 lines$'],
		[]),
	('safe-delete-class-used', dict(MakeSafeDeleteCopy(), **{
		#A class's usages are its members' too, and its labels in assembly; a class derived from it uses it.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '17:8', 'DEBUG': 'edit:safedelete'}),
		[r'^Safe Delete: CShape \[CShape\], 12 symbols, 5 usages, 1 in assembly$', r'^  used at Shape\.h 31:24$', r'^  used at Main\.cpp 6:2$',
		 r'^  used at Main\.cpp 18:29$', r'^Safe Delete: not deleted, it is used$'],
		[r'^Status: Deleted']),
	('safe-delete-overridden', dict(MakeSafeDeleteCopy(), **{
		#A virtual function a derived class overrides: the override's declaration and definition are usages.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '27:17', 'DEBUG': 'edit:safedelete'}),
		[r'^Safe Delete: CShape::Draw \[CShape::Draw\(\)\], 1 symbol, 2 usages \(1 function overrides it\)$', r'^  used at Shape\.h 34:7$', r'^  used at Shape\.cpp 65:15$',
		 r'^Safe Delete: not deleted, it is used$'],
		[r'^Status: Deleted']),
	('safe-delete-override', dict(MakeSafeDeleteCopy(), **{
		#The override itself overrides nothing below it: it has no usages and goes.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '34:8', 'DEBUG': 'edit:renamesearch:---s,edit:safedelete,printdisklines:Shape.h'}),
		[r'^Safe Delete: CCircle::Draw \[CCircle::Draw\(\)\], 1 symbol, no usages$', r'^Status: Deleted CCircle::Draw: 2 declarations and definitions in 2 files\.$',
		 r'^31\|class CCircle : public CShape$', r'^33\|public:$', r'^34\|\};$'],
		[r'override;', r'not deleted']),
	('safe-delete-assembly', dict(MakeSafeDeleteCopy(), **{
		#A function only hand written assembly calls, by its label, is used.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '24:9', 'DEBUG': 'edit:safedelete'}),
		[r'^Safe Delete: CShape::Kill \[CShape::Kill\(\)\], 1 symbol, 1 in assembly$', r'^Safe Delete: not deleted, it is used$'],
		[r'^Status: Deleted']),
	('safe-delete-others', dict(MakeSafeDeleteCopy(), **{
		#An enumerator with its line; a typedef; a function that only calls itself, its declaration and its definition; a
		#global's extern declaration and its definition; a class only ever declared; a local.  A parameter is Change
		#Signature's.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:26:20,edit:safedelete,edit:caret:52:13,edit:safedelete,edit:caret:50:7,edit:safedelete,edit:caret:49:13,edit:safedelete,'
		         'edit:caret:12:3,edit:safedelete,edit:caret:5:8,edit:safedelete,edit:open:Main.cpp,edit:caret:9:10,edit:safedelete,'
		         'printdisklines:Shape.h,printdisklines:Shape.cpp,printdisklines:Main.cpp'}),
		[r"^Safe Delete: iScale \[@.*Shape\.h:26:14:iScale\] can't be deleted: a parameter is removed by changing its function's signature$",
		 r'^Status: Deleted giUnused: 2 declarations and definitions in 2 files\.$',
		 r'^Status: Deleted UnusedFunction: 2 declarations and definitions in 2 files, with 2 things declared in it\.$',
		 r'^Status: Deleted TSpare: 1 declaration in 1 file\.$', r'^Status: Deleted EShapeKind::SK_Spare: 1 declaration in 1 file\.$',
		 r'^Status: Deleted CUnused: 1 declaration in 1 file\.$', r'^Status: Deleted iUnusedLocal: 1 declaration in 1 file\.$',
		 r'^Disk lines Shape\.h:$', r'^2\|#define __SHAPE_H__$', r'^3\|$', r'^4\|$', r'^5\|enum EShapeKind$', r'^7\|\tSK_Circle,$', r'^8\|\tSK_Square,$', r'^9\|\};$',
		 r'^42\|\};$', r'^43\|$', r'^44\|$', r'^45\|int\t\tUsedFunction\(int iValue\);$', r'^46\|$', r'^47\|$', r'^48\|#endif // __SHAPE_H__$', r'^48 lines$',
		 r'^Disk lines Shape\.cpp:$', r'^1\|#include "Shape\.h"$', r'^2\|$', r'^3\|$', r'^4\|/+$', r'^8\|void CShape::Init\(void\)$',
		 r'^91\|int UsedFunction\(int iValue\)$', r'^94 lines$',
		 r'^Disk lines Main\.cpp:$', r'^8\|\tEShapeKind\teKind;$', r'^9\|$', r'^10\|\t//UnusedFunction'],
		[r'not deleted', r'Safe Delete stopped', r'\|.*giUnused', r'\|.*SK_Spare', r'\|.*CUnused', r'\|typedef', r'\|.*iUnusedLocal', r'\|int UnusedFunction']),
	('safe-delete-searches', dict(MakeSafeDeleteCopy(), **{
		#As IntelliJ's searches: the name in a comment or a string is a usage when they are searched, and not otherwise.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '50:7',
		'DEBUG': 'edit:safedeletesearch:c-,edit:safedelete,edit:caret:49:13,edit:safedelete,edit:safedeletesearch:--,edit:safedeletable'}),
		[r'^Safe Delete: UnusedFunction \[UnusedFunction\(int\)\], 3 symbols, 1 in comments and strings$', r'^Safe Delete: not deleted, it is used$',
		 r'^Safe Delete: TSpare \[TSpare\], 1 symbol, 1 in comments and strings$', r'^Safe Delete: not deleted, it is used$',
		 r'^Safe Delete: TSpare \[TSpare\], 1 symbol, no usages$'],
		[r'^Status: Deleted']),
	('safe-delete-dialog', dict(MakeSafeDeleteCopy(), **{
		#The dialog, then the deletion, when nothing uses it.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '26:9', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'safedelete,answered:1,printstatus'}),
		[r'^Dialog \[Safe Delete\] answered OK$', r'^Status: Deleted CShape::Unused: 2 declarations and definitions in 2 files, with 3 things declared in it\.$'],
		[r'Usages Detected']),
	('safe-delete-dialog-usages', dict(MakeSafeDeleteCopy(), **{
		#Used: Usages Detected says how much, and View Usages shows them in Find Usages with nothing deleted.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '23:9', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|View Usages',
		'DEBUG': 'safedelete,answered:2,edit:printusages,printstatus'}),
		[r'^Dialog \[Safe Delete\] answered OK$', r'^Dialog \[Usages Detected\] answered View Usages$', r'Main\.cpp'],
		[r'^Status: Deleted']),
	('safe-delete-dialog-anyway', dict(MakeSafeDeleteCopy(), **{
		#Delete Anyway deletes it; Cancel in the first dialog does nothing.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '23:9', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|OK|Cancel',
		'DEBUG': 'safedelete,answered:2,printstatus,edit:caret:25:9,safedelete,answered:3,printstatus'}),
		[r'^Dialog \[Usages Detected\] answered OK$', r'^Status: Deleted CShape::Init: 2 declarations and definitions in 2 files\.  Left using it: 2 usages\.$',
		 r'^Dialog \[Safe Delete\] answered Cancel$', r'^Status line: Deleted CShape::Init'],
		[r'Deleted CShape::Area']),
	('safe-delete-namespace', dict(MakeSafeDeleteCopy(), **{
		#A namespace goes with what is in it, a function's definition outside it too; an alias.
		'WAIT_INDEX': '1', 'OPEN': 'Extra.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:46:8,edit:safedelete,edit:caret:5:12,edit:safedelete,printdisklines:Extra.h,printdisklines:Extra.cpp'}),
		[r'^Safe Delete: TWord \[TWord\], 1 symbol, no usages$', r'^Status: Deleted TWord: 1 declaration in 1 file\.$',
		 r'^Safe Delete: NSpare \[NSpare\], 6 symbols, no usages$', r'^  takes out Extra\.h 5:1-17:1$', r'^  takes out Extra\.cpp 4:1-14:1$',
		 r'^Status: Deleted NSpare: 2 declarations and definitions in 2 files, with 5 things declared in it\.$',
		 r'^Disk lines Extra\.h:$', r'^2\|#define __EXTRA_H__$', r'^3\|$', r'^4\|$', r'^5\|template<class M>$', r'^6\|M Largest\(M a, M b\)$',
		 r'^31\|\}$', r'^32\|$', r'^33\|$', r'^34\|#endif // __EXTRA_H__$', r'^34 lines$',
		 r'^Disk lines Extra\.cpp:$', r'^1\|#include "Extra\.h"$', r'^2\|$', r'^3\|$', r'^4\|/+$', r'^8\|int UseBox\(void\)$'],
		[r'\|.*NSpare', r'\|.*TWord', r'\|.*CInside', r'not deleted']),
	('safe-delete-template', dict(MakeSafeDeleteCopy(), **{
		#In templates (Refactorings.txt, "In a template"): a function template goes with its template<...> line; a class
		#template's member defined outside it goes with its own template<...> and the comment over it; what an instance
		#uses is used (CBox<int>'s Set and Get in Extra.cpp), and so is what the template's own body uses (mItem).
		'WAIT_INDEX': '1', 'OPEN': 'Extra.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:31:8,edit:safedelete,edit:caret:28:5,edit:safedelete,edit:caret:25:8,edit:safedelete,edit:caret:18:4,edit:safedelete,'
		         'printdisklines:Extra.h'}),
		[r'^Safe Delete: CBox::Set \[CBox::Set\(M\)\], 4 symbols, 1 usage$', r'^  takes out Extra\.h 31:1-32:1$', r'^  takes out Extra\.h 35:1-46:1$', r'^  used at Extra\.cpp 22:7$',
		 r'^Safe Delete: not deleted, it is used$',
		 r'^Safe Delete: CBox::mItem \[CBox::mItem\], 1 symbol, 2 usages$', r'^  used at Extra\.h 30:24$', r'^  used at Extra\.h 42:2$', r'^Safe Delete: not deleted, it is used$',
		 r'^Safe Delete: CBox \[CBox\], 8 symbols, 3 usages$', r'^Safe Delete: not deleted, it is used$',
		 r'^Safe Delete: Largest \[Largest\(M, M\)\], 4 symbols, no usages$', r'^  takes out Extra\.h 17:1-24:1$',
		 r'^Status: Deleted Largest: 1 declaration in 1 file, with 3 things declared in it\.$',
		 r'^14\|\}$', r'^15\|$', r'^16\|$', r'^17\|template<class M>$', r'^18\|class CBox$', r'^42 lines$'],
		[r'\|.*Largest']),
	('safe-delete-template-instance', dict(MakeRenameTemplateCopy(), **{
		#A member only a template's body names, through an instance, is used (T1): CFoo::Init by CHolder<CFoo>'s
		#mp->Init(), as well as by main.
		'WAIT_INDEX': '1', 'OPEN': 'Foo.h', 'CARET': '10:8', 'DEBUG': 'edit:safedelete'}),
		[r'^Safe Delete: CFoo::Init \[CFoo::Init\(\)\], 1 symbol, 2 usages, 2 in assembly$', r'^  used at Holder\.h 11:25$', r'^Safe Delete: not deleted, it is used$'],
		[r'^Status: Deleted']),
	('safe-delete-undo', dict(MakeSafeDeleteCopy(), **{
		#One Undo Across Files puts back everything a deletion took out, in every file.
		'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '39:8',
		'DEBUG': 'edit:safedelete,edit:undoall,edit:renamesearch:---s,edit:caret:21:9,edit:safedelete,printdisklines:Shape.h,printdisklines:Shape.cpp'}),
		[r'^Status: Deleted CLonely: 3 declarations and definitions in 2 files, with 3 things declared in it\.$', r'^Status: Deleted CShape::miSpare: 1 declaration in 1 file\.$',
		 r'^37\|//Nothing uses it but itself\.$', r'^38\|class CLonely$', r'^54 lines$', r'^74\|void CLonely::Init\(void\)$', r'^84\|int CLonely::Get\(void\)$', r'^108 lines$'],
		[r'Safe Delete stopped']),
	('introduce-variable-all', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Variable (Refactorings.txt, 5) for an expression written twice in a function: declared with the
		#function's declarations, its name in their column, assigned before the first statement that uses it, and both
		#occurrences replaced.  The caret is left on the assignment.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:66:29-66:40,edit:introduce:iDouble,edit:print,printdisklines:Calc.cpp'}),
		[r'^Introduce Variable: \[miScale \* 2\] is \[int\], named \[iScale\], 2 occurrences$', r'^Status: Introduced int iDouble = miScale \* 2: 2 occurrences replaced\.$',
		 r'Calc\.cpp \(focus\) line 67: \[\tiDouble = miScale \* 2;\]$',
		 r'^62\|\tint\t\tiResult;$', r'^63\|\tCCalc\*\tpcOther;$', r'^64\|\tint\t\tiDouble;$', r'^65\|$', r'^66\|\tpcOther = this;$', r'^67\|\tiDouble = miScale \* 2;$',
		 r'^68\|\tiResult = Scaled\(iExtra\) \+ iDouble;$', r'^71\|\t\tiResult \+= iDouble;$', r'^158 lines$'],
		[r"can't"]),
	('introduce-variable-one', dict(MakeIntroduceVariableCopy(), **{
		#The selected occurrence alone: the other is left as it is written.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:69:14-69:25,edit:introduceone:iDouble,printdisklines:Calc.cpp'}),
		[r'^Status: Introduced int iDouble = miScale \* 2: 1 occurrence replaced\.$',
		 r'^64\|\tint\t\tiDouble;$', r'^67\|\tiResult = Scaled\(iExtra\) \+ miScale \* 2;$', r'^70\|\t\tiDouble = miScale \* 2;$', r'^71\|\t\tiResult \+= iDouble;$'],
		[r"can't"]),
	('introduce-variable-declared-type', dict(MakeIntroduceVariableCopy(), **{
		#A call's type is what its function's declaration says it returns (uint32, not unsigned int), and its name is
		#from the function's.  The call on another object (pcOther->GetCount()) is another expression.  Used in an if's
		#condition and after it, it is assigned before the if.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:67:6-67:16,edit:introduce:uiCount,printdisklines:Calc.cpp'}),
		[r'^Introduce Variable: \[GetCount\(\)\] is \[uint32\], named \[uiCount\], 2 occurrences$', r'^Status: Introduced uint32 uiCount = GetCount\(\): 2 occurrences replaced\.$',
		 r'^64\|\tuint32\tuiCount;$', r'^67\|\tiResult = Scaled\(iExtra\) \+ miScale \* 2;$', r'^68\|\tuiCount = GetCount\(\);$', r'^69\|\tif \(uiCount > 4\)$',
		 r'^72\|\t\tiResult \+= pcOther->GetCount\(\);$', r'^76\|\treturn iResult \+ uiCount;$'],
		[r"can't"]),
	('introduce-variable-statement', dict(MakeIntroduceVariableCopy(), **{
		#A statement that is only the expression becomes the assignment.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:72:2-72:11,edit:introduce:szName,printdisklines:Calc.cpp'}),
		[r'^Introduce Variable: \[GetName\(\)\] is \[char\*\], named \[szName\], 1 occurrence$', r'^Status: Introduced char\* szName = GetName\(\): the statement is its assignment\.$',
		 r'^64\|\tchar\*\tszName;$', r'^73\|\tszName = GetName\(\);$', r'^74\|\tInit\(\);$', r'^157 lines$'],
		[r"can't"]),
	('introduce-variable-no-declarations', dict(MakeIntroduceVariableCopy(), **{
		#A function with no declarations gets them: the variable's, and a blank line.  Its type is the compiler's where
		#no declaration says it (uint32 + int).
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:84:13-84:27,edit:introduce:uiNext,printdisklines:Calc.cpp'}),
		[r'^Introduce Variable: \[GetCount\(\) \+ 1\] is \[unsigned int\], named \[uiCount\], 1 occurrence$',
		 r'^82\|void CCalc::Reset\(void\)$', r'^83\|\{$', r'^84\|\tunsigned int\tuiNext;$', r'^85\|$', r'^86\|\tuiNext = GetCount\(\) \+ 1;$', r'^87\|\tmuiCount = uiNext;$', r'^88\|\}$'],
		[r"can't"]),
	('introduce-variable-realign', dict(MakeIntroduceVariableCopy(), **{
		#A type longer than the declarations' column allows moves their names over to the new one's.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:98:17-98:27,edit:introduce:uiBig,printdisklines:Calc.cpp'}),
		[r'^Introduce Variable: \[BigValue\(\)\] is \[unsigned long long\], named \[uiBigValue\], 1 occurrence$',
		 r'^94\|\tint\t\t\t\t\tiResult;$', r'^95\|\tchar\*\t\t\t\tsz;$', r'^96\|\tunsigned long long\tuiBig;$', r'^97\|$', r'^98\|\tsz = GetName\(\);$', r'^99\|\tuiBig = BigValue\(\);$',
		 r'^100\|\tiResult = \(int\)uiBig \+ \(sz \? 1 : 0\);$'],
		[r"can't"]),
	('introduce-variable-template', dict(MakeIntroduceVariableCopy(), **{
		#In a template (Refactorings.txt, T3): the type is in the template's own words, M* for Top() whatever CStack is
		#instantiated with; what only the template's argument decides (Top()->miScale) is refused.  The header is parsed
		#in the unit that includes it.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.h',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:47:26-47:40,edit:introducible,edit:selecttext:47:26-47:31,edit:introduce:pTop,printdisklines:Calc.h'}),
		[r"^Introduce Variable: can't: its type depends on the template's arguments$",
		 r'^Introduce Variable: \[Top\(\)\] is \[M\*\], named \[pTop\], 1 occurrence$', r'^Status: Introduced M\* pTop = Top\(\): 1 occurrence replaced\.$',
		 r'^45\|\tint\tiResult;$', r'^46\|\tM\*\tpTop;$', r'^47\|$', r'^48\|\tpTop = Top\(\);$', r'^49\|\tiResult = miDepth \* 2 \+ pTop->miScale;$'],
		[r'CCalc\*']),
	('introduce-variable-refused', dict(MakeIntroduceVariableCopy(), **{
		#What can't be: an expression with no value; a selection that isn't one expression; no selection.  A name the
		#function uses is refused.  A variable's own name is an expression, and the places it is assigned aren't
		#occurrences of its value.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:introducible,edit:selecttext:73:2-73:8,edit:introducible,edit:selecttext:74:9-74:18,edit:introducible,edit:selecttext:74:9-74:16,edit:introducible,'
		         'edit:selecttext:66:29-66:40,edit:introduce:iExtra,edit:introduce:2bad,printdisklines:Calc.cpp'}),
		[r'^Introduce Variable: nothing is selected$', r"^Introduce Variable: can't: it has no value \(its type is void\)$", r"^Introduce Variable: can't: the selection isn't an expression$",
		 r'^Introduce Variable: \[iResult\] is \[int\], named \[iResult2\], 1 occurrence$',
		 r'^Status: Introduce Variable: The function uses that name already\.$', r"^Status: Introduce Variable: It isn't a name\.$", r'^156 lines$'],
		[r'^Status: Introduced']),
	('introduce-variable-undo', dict(MakeIntroduceVariableCopy(), **{
		#One undo takes all of it back: the declaration, the assignment and the occurrences.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:selecttext:66:29-66:40,edit:introduce:iDouble,edit:undo,edit:renamesearch:---s,edit:selecttext:72:2-72:11,edit:introduce:szName,printdisklines:Calc.cpp'}),
		[r'^Status: Introduced int iDouble', r'^62\|\tint\t\tiResult;$', r'^63\|\tCCalc\*\tpcOther;$', r'^64\|\tchar\*\tszName;$', r'^65\|$',
		 r'^67\|\tiResult = Scaled\(iExtra\) \+ miScale \* 2;$', r'^157 lines$'],
		[r'\|.*iDouble']),
	('introduce-variable-dialog', dict(MakeIntroduceVariableCopy(), **{
		#The dialog: the suggested name typed over, every occurrence replaced as its box says to start with.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:iTwice', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:selecttext:66:29-66:40,introducevariable,answered:1,printstatus'}),
		[r'^Dialog \[Introduce Variable\] answered OK$', r'^Status: Introduced int iTwice = miScale \* 2: 2 occurrences replaced\.$'],
		[r"can't"]),
	('introduce-variable-choose', dict(MakeIntroduceVariableCopy(), **{
		#With nothing selected the expressions the caret is in are offered, the innermost first, in a popup under it;
		#the one chosen is selected and introduced.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp', 'CARET': '66:31', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'edit:introducecandidates,introducevariable,edit:printpopup,edit:introducechoose:1,edit:printselection,edit:introducible'}),
		[r'^Expressions: miScale \| miScale \* 2 \| Scaled\(iExtra\) \+ miScale \* 2 \| iResult = Scaled\(iExtra\) \+ miScale \* 2$',
		 r'^Popup: Expressions \| miScale \| miScale \* 2 \| Scaled\(iExtra\) \+ miScale \* 2 \| iResult = Scaled\(iExtra\) \+ miScale \* 2$',
		 r'^Selection: \[miScale \* 2\] on line 66', r'^Introduce Variable: \[miScale \* 2\] is \[int\], named \[iScale\], 2 occurrences$'],
		[r"can't"]),
	('introduce-variable-warnings', dict(MakeIntroduceVariableCopy(), **{
		#What replacing every occurrence could change is found, and shown before all are replaced: something the
		#expression names is changed between its occurrences; the expression is a call.  Cancel in Problems Detected
		#introduces nothing; Continue replaces them all.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|Cancel|OK|OK',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:122:11-122:20,edit:introducible,edit:selecttext:125:18-125:30,edit:introducible,edit:selecttext:122:11-122:20,introducevariable,answered:2,printstatus,'
				 'edit:selecttext:122:11-122:20,introducevariable,answered:4,printstatus'}),
		[r'^Introduce Variable: \[iBase \* 2\] is \[int\], named \[iBase2\], 2 occurrences$',
		 r'^  Replacing all: iBase, which the expression names, is changed at line 123, between its first occurrence and its last: they may not all be the same value\.$',
		 r'^Introduce Variable: \[Next\(&iStep\)\] is \[int\], named \[iNext\], 2 occurrences$',
		 r'^  Replacing all: The expression does something each time it is worked out \(a call, a \+\+ or an assignment\): one variable for every occurrence does it once\.$',
		 r'^Dialog \[Problems Detected\] answered Cancel$', r'^Status line: $', r'^Dialog \[Problems Detected\] answered OK$',
		 r'^Status: Introduced int iBase2 = iBase \* 2: 2 occurrences replaced\.$'],
		[]),
	('change-signature-constructor-bare', dict(MakeConstructorsCopy(), **{
		#A constructor that takes nothing given a parameter: an object made with no brackets (a variable, new) gets
		#them with the value, as one made with empty brackets does.
		'WAIT_INDEX': '1', 'OPEN': 'Box.h', 'CARET': '20:3',
		'DEBUG': 'edit:renamesearch:---s,edit:changeable,edit:changesignature:+int|iSize|7|,printdisklines:Box.h,printdisklines:Box.cpp'}),
		[r'^Change Signature: CLid::CLid \[CLid::CLid\(\)\]  \(\): 2 declarations and definitions, 0 overrides, 0 family, 3 calls, 0 other uses$',
		 r'^Status: Changed CLid::CLid.s signature: 2 declarations and definitions, 3 calls in 2 files\.$',
		 r'^20\|\tCLid\(int iSize\);$', r'^44\|CLid::CLid\(int iSize\)$', r'^56\|\tCLid\tcLid\(7\);$', r'^57\|\tCLid\*\tpcLid;$', r'^58\|\tCLid\tcOther = CLid\(7\);$', r'^60\|\tpcLid = new CLid\(7\);$'],
		[]),
	('change-signature-constructor', dict(MakeConstructorsCopy(), **{
		#Change Signature of a constructor: an object made with arguments is changed as a call is, however it is
		#made: a variable's ( and {, new, a temporary, a base's and a member's initialiser.
		'WAIT_INDEX': '1', 'OPEN': 'Box.h', 'CARET': '11:3',
		'DEBUG': 'edit:renamesearch:---s,edit:changeable,edit:changesignature:1;0;+int|iDepth|9|,printdisklines:Box.h,printdisklines:Box.cpp'}),
		[r'^Change Signature: CBox::CBox \[CBox::CBox\(int, int\)\]  \(int iWidth, int iHeight\): 2 declarations and definitions, 0 overrides, 0 family, 7 calls, 0 other uses$',
		 r'^Status: Changed CBox::CBox.s signature: 2 declarations and definitions, 7 calls in 2 files\.$',
		 r'^11\|\tCBox\(int iHeight, int iWidth, int iDepth\);$',
		 r'^8\|CBox::CBox\(int iHeight, int iWidth, int iDepth\)$', r'^19\|CCrate::CCrate\(int iSize\) : CBox\(iSize, iSize, 9\), mcInner\(2, 1, 9\)$',
		 r'^30\|\tCBox\tcBox\(4, 3, 9\);$', r'^31\|\tCBox\tcOther\{6, 5, 9\};$', r'^33\|\tCBox\tcThird = CBox\(8, 7, 9\);$', r'^35\|\tpcBox = new CBox\(10, 9, 9\);$',
		 r'CBox\(12, 11, 9\)\.miHeight;$'],
		[]),
	('unify-parameter-names', dict(MakeChangeSignatureCopy(), **{
		#Unify Parameter Names: the definition calls its parameters iR, iG and iB where the class declares iRed, iGreen
		#and iBlue.  From the declaration the definition takes its names, in its body too; one whose names are the
		#same already is left.  Show Context Actions offers it on a function declared and defined apart.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:11:9,edit:contextactions,edit:unifynames,edit:caret:12:18,edit:unifynames,printdisklines:Paint.h,printdisklines:Paint.cpp'}),
		[r'^Context actions: Rename\.\.\. \| Change Signature\.\.\. \| Unify Parameter Names \| ',
		 r"^Status: Unified the names of CBrush::Mix's parameters: 3 were named differently, and are as they are named here now\.$",
		 r'^Status: The parameters of CBrush::Draw have the same names wherever it is declared and defined\.$',
		 r'^11\|\tint\t\t\t\tMix\(int iRed, int iGreen, int iBlue\);$', r'^19\|int CBrush::Mix\(int iRed, int iGreen, int iBlue\)$', r'^23\|\tiTotal = iRed \+ iGreen \* 2;$',
		 r'^24\|\treturn iTotal \+ iBlue \+ iRed;$'],
		[]),
	('unify-parameter-names-definition', dict(MakeChangeSignatureCopy(), **{
		#From the definition, the declaration takes the definition's names.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:19:14,edit:unifynames,printdisklines:Paint.h,printdisklines:Paint.cpp'}),
		[r"^Status: Unified the names of CBrush::Mix's parameters: 3 were named differently, and are as they are named here now\.$",
		 r'^11\|\tint\t\t\t\tMix\(int iR, int iG, int iB\);$', r'^19\|int CBrush::Mix\(int iR, int iG, int iB\)$', r'^23\|\tiTotal = iR \+ iG \* 2;$'],
		[]),
	('change-signature-reorder', dict(MakeChangeSignatureCopy(), **{
		#Change Signature (Refactorings.txt, 2): a function's parameters put in another order and one renamed.  The
		#declaration and the definition each keep what they wrote (the definition names them iR, iG and iB), the renamed
		#one is renamed where the body uses it, and the call's arguments follow their parameters.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '11:9',
		'DEBUG': 'edit:renamesearch:---s,edit:changesignature:2;0=iR2;1,printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^Change Signature: CBrush::Mix \[CBrush::Mix\(int, int, int\)\] int \(int iRed, int iGreen, int iBlue\): 2 declarations and definitions, 0 overrides, 0 family, 1 calls, 0 other uses$',
		 r'^  declared Paint\.h:11 ', r'^  defined Paint\.cpp:19 ', r'^  called Main\.cpp:16 with 3 arguments$',
		 r'^New signature: int CBrush::Mix\(int iBlue, int iR2, int iGreen\)$',
		 r"^Status: Changed CBrush::Mix's signature: 2 declarations and definitions, 1 call in 3 files\.$",
		 r'^11\|\tint\t\t\t\tMix\(int iBlue, int iR2, int iGreen\);$', r'^19\|int CBrush::Mix\(int iBlue, int iR2, int iGreen\)$', r'^23\|\tiTotal = iR2 \+ iGreen \* 2;$',
		 r'^24\|\treturn iTotal \+ iBlue \+ iR2;$', r'^16\|\tiMixed = cBrush\.Mix\(3, 1, Next\(\)\);$'],
		[r'^Conflict', r'not changed']),
	('change-signature-from-call', dict(MakeChangeSignatureCopy(), **{
		#From a call of it: the same function, the same change.
		'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '16:19',
		'DEBUG': 'edit:renamesearch:---s,edit:changesignature:2;0;1,printdisklines:Paint.h,printdisklines:Main.cpp'}),
		[r'^Change Signature: CBrush::Mix ', r'^11\|\tint\t\t\t\tMix\(int iBlue, int iRed, int iGreen\);$', r'^16\|\tiMixed = cBrush\.Mix\(3, 1, Next\(\)\);$'],
		[r'^Conflict']),
	('change-signature-add', dict(MakeChangeSignatureCopy(), **{
		#A new parameter between two: its type and name in the declaration and the definition, and the value given for it
		#in each call, where its place is.  A parameter added to one that had none replaces (void).
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:10:9,edit:changesignature:0;+bool|bFast|true|;1,edit:caret:13:9,edit:changesignature:+int|iValue|7|,'
		         'printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^New signature: void CBrush::Init\(int iSize, bool bFast, const char\* szName = 0\)$', r'^New signature: void CBrush::Clear\(int iValue\)$',
		 r'^10\|\tvoid\t\t\tInit\(int iSize, bool bFast, const char\* szName = 0\);$', r'^13\|\tvoid\t\t\tClear\(int iValue\);$',
		 r'^8\|void CBrush::Init\(int iSize, bool bFast, const char\* szName\)$', r'^42\|void CBrush::Clear\(int iValue\)$',
		 r'^14\|\tcBrush\.Init\(4, true\);$', r'^15\|\tcSpray\.Init\(2, true, "wide"\);$', r'^20\|\tcBrush\.Clear\(7\);$'],
		[r'^Conflict']),
	('change-signature-override', dict(MakeChangeSignatureCopy(), **{
		#A virtual function is changed with what overrides it, from either of them, in the calls on both classes.  A new
		#parameter's default value is in each one's declaration (a call on the derived class uses that one's), not in
		#the definitions; the calls there are leave it to the default.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:changesignature:0;1;+int|iZ||9,printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^Change Signature: CSpray::Draw \[CSpray::Draw\(int, int\)\] void \(int iX, int iY\): 4 declarations and definitions, 1 overrides, 0 family, 2 calls, 0 other uses$',
		 r"^Status: Changed CSpray::Draw's signature: 4 declarations and definitions \(1 override\), 2 calls in 3 files\.$",
		 r'^12\|\tvirtual void\tDraw\(int iX, int iY, int iZ = 9\);$', r'^20\|\tvoid\tDraw\(int iX, int iY, int iZ = 9\) override;$',
		 r'^32\|void CBrush::Draw\(int iX, int iY, int iZ\)$', r'^52\|void CSpray::Draw\(int iX, int iY, int iZ\)$',
		 r'^18\|\tpcBrush->Draw\(iMixed, 2\);$', r'^19\|\tcSpray\.Draw\(3, 4\);$'],
		[r'^Conflict']),
	('change-signature-conflicts', dict(MakeChangeSignatureCopy(), **{
		#What stands in the way stops it: a parameter taken out that the bodies still use, an argument taken out that
		#does something.  Anyway, it is changed.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:12:15,edit:changesignature:1,edit:caret:11:9,edit:changesignature:0;2,edit:changesignatureanyway:0;2,'
		         'printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^Conflict: CBrush::Draw still uses the parameter taken out, iX, at Paint\.cpp:34\.$', r'^Conflict: CSpray::Draw still uses the parameter taken out, iX, at Paint\.cpp:54\.$',
		 r'^Change Signature: not changed$',
		 r'^Conflict: CBrush::Mix still uses the parameter taken out, iG, at Paint\.cpp:23\.$',
		 r'^Conflict: The argument taken out of the call at Main\.cpp:16 does something \(a call, an assignment or the like\), which goes with it\.$',
		 r'^Change Signature: not changed$', r"^Status: Changed CBrush::Mix's signature: ",
		 r'^11\|\tint\t\t\t\tMix\(int iRed, int iBlue\);$', r'^12\|\tvirtual void\tDraw\(int iX, int iY\);$', r'^19\|int CBrush::Mix\(int iRed, int iBlue\)$',
		 r'^16\|\tiMixed = cBrush\.Mix\(1, 3\);$'],
		[]),
	('change-signature-defaults', dict(MakeChangeSignatureCopy(), **{
		#A default value taken away is written into the call that left its argument to it; one given to a parameter is
		#written in the declaration, not the definition; a parameter with a default value before one without stands in
		#the way.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:10:9,edit:changesignature:1;0,edit:changesignature:0;1|||,edit:caret:11:9,edit:changesignature:0;1;2|||3,'
		         'printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^Conflict: A parameter with a default value is before one without: iSize needs one, or to come first\.$', r'^Change Signature: not changed$',
		 r'^New signature: void CBrush::Init\(int iSize, const char\* szName\)$', r'^New signature: int CBrush::Mix\(int iRed, int iGreen, int iBlue = 3\)$',
		 r'^10\|\tvoid\t\t\tInit\(int iSize, const char\* szName\);$', r'^11\|\tint\t\t\t\tMix\(int iRed, int iGreen, int iBlue = 3\);$',
		 r'^19\|int CBrush::Mix\(int iRed, int iGreen, int iBlue\)$', r'^14\|\tcBrush\.Init\(4, 0\);$', r'^15\|\tcSpray\.Init\(2, "wide"\);$'],
		[]),
	('change-signature-return-name', dict(MakeChangeSignatureCopy(), **{
		#What it returns, its name, and a parameter's type and default value, with the rest: the return type where each
		#writes it (the declaration's name stays in its column), the name by Rename, the type where each declares the
		#parameter, each with its own name unless it is renamed.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '11:9',
		'DEBUG': 'edit:renamesearch:---s,edit:changesignature:0|iRedder|long|;1;2||short|5;return=long;name=Stir,'
		         'printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^New signature: long CBrush::Stir\(long iRedder, int iGreen, short iBlue = 5\)$',
		 r"^Status: Changed CBrush::Mix's signature: 2 declarations and definitions, 1 call in 3 files\.  It is now Stir\.$",
		 r'^11\|\tlong\t\t\tStir\(long iRedder, int iGreen, short iBlue = 5\);$', r'^19\|long CBrush::Stir\(long iRedder, int iGreen, short iBlue\)$',
		 r'^23\|\tiTotal = iRedder \+ iGreen \* 2;$', r'^16\|\tiMixed = cBrush\.Stir\(1, Next\(\), 3\);$'],
		[r'^Conflict', r'\|.*Mix\(']),
	('change-signature-other-conflicts', dict(MakeChangeSignatureCopy(), **{
		#Another function of its name with the new parameters; a function named without being called (a pointer to it);
		#a call whose value is used when it is to return nothing.  With none of its parameters left a declaration says
		#(void) and a call ().
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:24:7,edit:changesignature:0;1;+int|iExtra|0|,edit:caret:26:7,edit:changesignature:return=void,'
		         'edit:caret:25:7,edit:changesignatureanyway:,printdisklines:Paint.h,printdisklines:Main.cpp'}),
		[r'^Conflict: There is another Blend with these parameters: Blend\(int, int, int\)\.$', r'^Change Signature: not changed$',
		 r"^Conflict: It is named without being called at Main\.cpp:4 \(its address, or a pointer to it\): what it is used as there isn't changed\.$",
		 r'^Conflict: What it returns is used by the call at Main\.cpp:16\.$', r'^Change Signature: not changed$',
		 r'^New signature: int Blend\(void\)$', r'^Conflict: Blend still uses the parameter taken out, iFirst, at Paint\.cpp:74\.$',
		 r'^24\|int\t\tBlend\(int iFirst, int iSecond\);$', r'^25\|int\t\tBlend\(void\);$', r'^26\|int\t\tNext\(void\);$', r'^21\|\treturn Blend\(1, 2\) \+ Blend\(\);$'],
		[]),
	('change-signature-refused', dict(MakeChangeSignatureCopy(), **{
		#What isn't a function, and what can't be a signature.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h',
		'DEBUG': 'edit:caret:8:8,edit:changeable,edit:caret:10:9,edit:changesignature:5,edit:changesignature:0;0,edit:changesignature:0;1;+int||1|,'
		         'edit:changesignature:0;1=iSize,edit:changesignature:0=2x;1,edit:changesignature:name=9lives'}),
		[r"^Change Signature: CBrush::miSize \[CBrush::miSize\] can't be changed: it isn't a function$", r'^Change Signature: The function has no such parameter: 5$',
		 r'^Change Signature: A parameter is given twice\.$', r'^Change Signature: A new parameter needs a type and a name\.$',
		 r'^Change Signature: Two parameters are named iSize\.$', r"^Change Signature: A parameter's name isn't a name: 2x$", r"^Change Signature: The function's name isn't a name\.$"],
		[r'^Status: Changed']),
	('change-signature-template-family', dict(MakeRenameTemplateCopy(), **{
		#In a template (Refactorings.txt, T2): CHolder<M>'s body calls CFoo::Init and CBar::Init by one name, so
		#they are changed together, from either, and the template's call is changed once.  CLone::Init, which no instance
		#calls, is left.
		'WAIT_INDEX': '1', 'OPEN': 'Foo.h', 'CARET': '10:8',
		'DEBUG': 'edit:renamesearch:---s,edit:changesignature:+int|iLevel|3|,printdisklines:Holder.h,printdisklines:Foo.h,printdisklines:Bar.h,printdisklines:Bar.cpp,'
		         'printdisklines:Lone.h,printdisklines:Main.cpp'}),
		[r'^Change Signature: CFoo::Init \[CFoo::Init\(\)\] void \(\): 4 declarations and definitions, 0 overrides, 1 family, 3 calls, 0 other uses$',
		 r"^Status: Changed CFoo::Init's signature: 4 declarations and definitions, 3 calls in 6 files\.  With CBar::Init, which a template calls with it by one name\.$",
		 r'^11\|\tvoid\tStart\(void\) \{ mp->Init\(3\); \}$', r'^Disk lines Foo\.h:$', r'^10\|\tvoid\tInit\(int iLevel\);$', r'^Disk lines Bar\.h:$', r'^10\|\tvoid\tInit\(int iLevel\);$',
		 r'^4\|void CBar::Init\(int iLevel\)$', r'^Disk lines Lone\.h:$', r'^10\|\tvoid\tInit\(void\);$',
		 r'^15\|\tcFoo\.Init\(3\);$', r'^16\|\tcBar\.Init\(3\);$', r'^17\|\tcLone\.Init\(\);$'],
		[r'^Conflict']),
	('change-signature-undo', dict(MakeChangeSignatureCopy(), **{
		#One Undo Across Files takes all of it back, in every file.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '11:9',
		'DEBUG': 'edit:changesignature:2;0=iR2;1,edit:undoall,edit:renamesearch:---s,edit:caret:13:9,edit:changesignature:+int|iValue|7|,'
		         'printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r"^Status: Changed CBrush::Mix's signature: ", r"^Status: Changed CBrush::Clear's signature: ",
		 r'^11\|\tint\t\t\t\tMix\(int iRed, int iGreen, int iBlue\);$', r'^19\|int CBrush::Mix\(int iR, int iG, int iB\)$', r'^23\|\tiTotal = iR \+ iG \* 2;$',
		 r'^16\|\tiMixed = cBrush\.Mix\(1, Next\(\), 3\);$'],
		[r'^[0-9]+[|].*iR2']),
	('change-signature-dialog', dict(MakeChangeSignatureCopy(), **{
		#The dialog: the parameters in its table, edited where they are, and the signature as it would be, as they are
		#changed (SIGNATURE_EDITS does what a user would); then the change.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '10:9', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK',
		'SIGNATURE_EDITS': 'print;select:0;name:iWidth;add;type:bool;name:bFast;value:true;print;check;function:Start;return:int;print',
		'DEBUG': 'changesignature,answered:1,printstatus,command:File.SaveAll,printdisklines:Paint.h,printdisklines:Paint.cpp,printdisklines:Main.cpp'}),
		[r'^Signature dialog: >int iSize \| const char\* szName = 0$', r'^Signature shown: void CBrush::Init\(int iSize, const char\* szName = 0\)$',
		 r'^Signature dialog: int iWidth \| >bool bFast \| const char\* szName = 0$', r'^Signature shown: void CBrush::Init\(int iWidth, bool bFast, const char\* szName = 0\)$',
		 r'^Signature dialog: OK \[\]$', r'^Signature shown: int CBrush::Start\(int iWidth, bool bFast, const char\* szName = 0\)$',
		 r'^Dialog \[Change Signature\] answered OK$', r"^Status line: Changed CBrush::Init's signature: 2 declarations and definitions, 2 calls in 3 files\.  It is now Start\.$",
		 r'^10\|\tint\t\t\t\tStart\(int iWidth, bool bFast, const char\* szName = 0\);$', r'^8\|int CBrush::Start\(int iWidth, bool bFast, const char\* szName\)$',
		 r'^10\|\tmiSize = iWidth \+ \(szName \? 1 : 0\);$', r'^14\|\tcBrush\.Start\(4, true\);$', r'^15\|\tcSpray\.Start\(2, true, "wide"\);$'],
		[r'^Conflict', r'Problems Detected']),
	('change-signature-dialog-moves', dict(MakeChangeSignatureCopy(), **{
		#Up, Down and Remove in the dialog, and what it refuses: a new parameter without a type, no name for the function.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '11:9', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'SIGNATURE_EDITS': 'select:2;up;up;print;down;print;select:0;remove;print;add;check;type:int;name:iNew;check;function:;check',
		'DEBUG': 'changesignature,answered:1,printstatus'}),
		[r'^Signature dialog: >int iBlue \| int iRed \| int iGreen$', r'^Signature dialog: int iRed \| >int iBlue \| int iGreen$', r'^Signature dialog: >int iBlue \| int iGreen$',
		 r'^Signature dialog: refused \[A new parameter needs a type and a name\.\]$', r'^Signature dialog: OK \[\]$', r'^Signature dialog: refused \[The function needs a name\.\]$',
		 r'^Dialog \[Change Signature\] answered Cancel$'],
		[r'Changed CBrush']),
	('change-signature-dialog-problems', dict(MakeChangeSignatureCopy(), **{
		#What stands in the way is shown in Problems Detected: Cancel there changes nothing, Continue changes it.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CARET': '12:15', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|Cancel|OK|OK',
		'SIGNATURE_EDITS': 'select:0;remove',
		'DEBUG': 'changesignature,answered:2,printstatus,changesignature,answered:4,printstatus'}),
		[r'^Dialog \[Change Signature\] answered OK$', r'^Conflict: CBrush::Draw still uses the parameter taken out, iX, at Paint\.cpp:34\.$', r'^Dialog \[Problems Detected\] answered Cancel$',
		 r'^Dialog \[Problems Detected\] answered OK$', r"^Status line: Changed CBrush::Draw's signature: 4 declarations and definitions \(1 override\), 2 calls in 3 files\.$"],
		[]),
	('change-signature-assembly', dict(MakeChangeSignatureCopy(), **{
		#Hand written assembly names a function by the compiler's label, which has its parameters' types in it where it
		#isn't a member or is overloaded: with a W65C816 configuration the labels are asked of the compiler (printlabel
		#prints one) before and after, and the assembly's are changed.  Blend(int, int)'s becomes _Blend__Fiic and
		#Blend(int, int, int)'s is left; a member that isn't overloaded keeps its label whatever its parameters.
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CONFIGURATION': 'Debug|W65C816',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:24:7,edit:printlabel,edit:changesignature:0;1;+char|cMode|\'a\'|,edit:caret:24:7,edit:printlabel,'
		         'edit:caret:11:9,edit:changesignature:2;0;1,printdisklines:PaintW65C816.asm,printdisklines:Main.cpp'}),
		[r'^Label: _Blend__Fii$', r"^Status: Changed Blend's signature: 2 declarations and definitions, 1 call in 3 files\.  2 labels in assembly\.$", r'^Label: _Blend__Fiic$',
		 r"^Status: Changed CBrush::Mix's signature: 2 declarations and definitions, 1 call in 3 files\.$",
		 r'^6\|\t\tjsr _CBrush__Init$', r'^7\|\t\tjsr _CBrush__Mix$', r'^8\|\t\tjsr _Blend__Fiic$', r'^9\|\t\tjsr _Blend__Fiii$', r'^10\|\t\tjsr _Next$',
		 r"^21\|\treturn Blend\(1, 2, 'a'\) \+ Blend\(1, 2, 3\);$"],
		[r'^Conflict']),
	('change-signature-assembly-host', dict(MakeChangeSignatureCopy(), **{
		#Whatever configuration is selected, both processors' labels are asked of the compiler and changed: the
		#W65C816's in its assembly and the 65C64's in its own (an int is i on both, though 16 bits on one).
		'WAIT_INDEX': '1', 'OPEN': 'Paint.h', 'CONFIGURATION': 'Debug|Win32', 'CARET': '24:7',
		'DEBUG': 'edit:renamesearch:---s,edit:printlabel,edit:changesignature:0;1;+char|cMode|\'a\'|,printdisklines:PaintW65C816.asm,printdisklines:Paint65C64.asm'}),
		[r'^Label: none$', r"^Status: Changed Blend's signature: 2 declarations and definitions, 1 call in 3 files\.  2 labels in assembly\.$",
		 r'^8\|\t\tjsr _Blend__Fiic$', r'^9\|\t\tjsr _Blend__Fiii$', r'^7\|\t\tjsr _Blend__Fiic$', r'^8\|\t\tjsr _Blend__Fiii$'],
		[]),
	('definitions-generate', dict(MakeDefinitionsCopy(), **{
		#Generate Definition: Fill is declared in CTank and defined nowhere.  Its definition goes in the file CTank's
		#others are in, after Init's (declared before it), two blank lines from what is around it, under a comment
		#like Init's, without its default value.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '14:8',
		'DEBUG': 'edit:renamesearch:---s,edit:generatedefinition,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r'^Status: Defined CTank::Fill in Tank\.cpp\.$', r'^Definitions: done$',
		 r'^14\|\tvoid\tFill\(int iAmount, int iRate = 1\);$',
		 r'^21\|\}$', r'^22\|$', r'^23\|$', r'^24\|/{74}$', r'^25\|//$', r'^26\|//$', r'^27\|/{74}$', r'^28\|void CTank::Fill\(int iAmount, int iRate\)$', r'^29\|\{$', r'^30\|\}$',
		 r'^31\|$', r'^32\|$', r'^33\|/{74}$', r'^35\|// Whether it holds all it can\.$', r'^38\|bool CTank::IsFull\(void\) const$'],
		[r'^Definitions: refused']),
	('definitions-generate-scheme', dict(MakeDefinitionsCopy([('Code', 'New Function Body', '## A comment of the template\'s.\n\t//To do: $FUNCTION_NAME.\n')]), **{
		#Generate Definition with the Workspace scheme's New Function Body (fileTemplates beside the solution): Fill's
		#body is that template's, its ## comment not written.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '14:8',
		'DEBUG': 'edit:renamesearch:---s,edit:generatedefinition,printdisklines:Tank.cpp'}),
		[r'^Status: Defined CTank::Fill in Tank\.cpp\.$', r'^28\|void CTank::Fill\(int iAmount, int iRate\)$', r'^29\|\{$', r'^30\|\t//To do: (CTank::)?Fill\.$', r'^31\|\}$'],
		[r'^Definitions: refused', r'A comment']),
	('definitions-generate-scheme-error', dict(MakeDefinitionsCopy([('Code', 'New Function Body', '#if ($BODY)\n$BODY\n')]), **{
		#A template with an error (an #if with no #end) stops Generate Definition, which says where the error is, and
		#nothing is written.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '14:8',
		'DEBUG': 'edit:renamesearch:---s,edit:generatedefinition,printdisklines:Tank.cpp'}),
		[r'The New Function Body template has an error: Line 1, column 1: #if has no #end', r'^21\|\}$'],
		[r'^Status: Defined', r'void CTank::Fill']),
	('definitions-generate-template', dict(MakeDefinitionsCopy(), **{
		#A function outside any class goes at the end of its header's source file; a class template's member in the
		#header, under the class's template<...>, after the member defined before it.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:49:8,edit:generatedefinition,edit:caret:38:8,edit:generatedefinition,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r'^Status: Defined MeasureTank in Tank\.cpp\.$', r'^Status: Defined CStore::Get in Tank\.h\.$',
		 r'^46\|\}$', r'^47\|$', r'^48\|$', r'^49\|/{74}$', r'^52\|/{74}$', r'^53\|template<class M>$', r'^54\|M\* CStore<M>::Get\(void\)$', r'^55\|\{$', r'^56\|\}$', r'^57\|$', r'^58\|$',
		 r'^59\|int\t\tMeasureTank\(CTank\* pcTank\);$',
		 r'^65\|\}$', r'^66\|$', r'^67\|$', r'^68\|/{74}$', r'^71\|/{74}$', r'^72\|int MeasureTank\(CTank\* pcTank\)$', r'^73\|\{$', r'^74\|\}$', r'^74 lines$'],
		[r'^Definitions: refused']),
	('definitions-move-out', dict(MakeDefinitionsCopy(), **{
		#Move Definition out of Class: a body on the declaration's line is opened out, one of several lines loses the
		#class's indentation, and const goes with it.  They are defined in the order the class declares them.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:16:8,edit:movedefinitionout,edit:caret:15:8,edit:movedefinitionout,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r"^Status: Moved CTank::Drain's definition out of its class, to Tank\.cpp\.$", r"^Status: Moved CTank::GetLevel's definition out of its class, to Tank\.cpp\.$",
		 r'^15\|\tint\t\tGetLevel\(void\) const;$', r'^16\|\tint\t\tDrain\(int iAmount\);$', r'^17\|\tbool\tIsFull\(void\) const;$',
		 r'^28\|int CTank::GetLevel\(void\) const$', r'^29\|\{$', r'^30\|\treturn miLevel;$', r'^31\|\}$', r'^32\|$', r'^33\|$', r'^34\|/{74}$',
		 r'^38\|int CTank::Drain\(int iAmount\)$', r'^39\|\{$', r'^40\|\tint\tiTaken;$', r'^41\|$', r'^42\|\tiTaken = \(iAmount < miLevel\) \? iAmount : miLevel;$',
		 r'^44\|\treturn iTaken;$', r'^45\|\}$', r'^53\|bool CTank::IsFull\(void\) const$'],
		[r'^Definitions: refused']),
	('definitions-move-out-template', dict(MakeDefinitionsCopy(), **{
		#A class template's member stays in the header, under the class's template<...>.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '39:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movedefinitionout,printdisklines:Tank.h'}),
		[r"^Status: Moved CStore::Set's definition out of its class, to Tank\.h\.$",
		 r'^39\|\tvoid\tSet\(M\* pcItem\);$', r'^53\|template<class M>$', r'^54\|void CStore<M>::Set\(M\* pcItem\)$', r'^55\|\{$', r'^56\|\tmcItem = \*pcItem;$', r'^57\|\}$',
		 r'^60\|int\t\tMeasureTank\(CTank\* pcTank\);$'],
		[r'^Definitions: refused']),
	('definitions-move-in', dict(MakeDefinitionsCopy(), **{
		#Move Definition into Class: one statement stays on the declaration's line, more go under it indented with the
		#class; a constructor's initialisers come with it and its parameter takes the name the definition gave it.
		#The definitions outside go with the comments over them.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:25:8,edit:movedefinitionin,edit:caret:24:8,edit:movedefinitionin,edit:caret:12:5,edit:movedefinitionin,'
		         'printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r"^Status: Moved CTank::Empty's definition into its class, from Tank\.cpp\.$", r"^Status: Moved CTank::IsFull's definition into its class, from Tank\.cpp\.$",
		 r"^Status: Moved CTank::CTank's definition into its class, from Tank\.cpp\.$",
		 r'^12\|\t\t\tCTank\(int iCapacity\) : miLevel\(0\), miSize\(iCapacity\) \{\}$',
		 r'^24\|\tbool\tIsFull\(void\) const \{ return miLevel >= miSize; \}$',
		 r'^25\|\tvoid\tEmpty\(void\)$', r'^26\|\t\{$', r'^27\|\t\tif \(miLevel > 0\)$', r'^28\|\t\t\{$', r'^29\|\t\t\tmiLevel = 0;$', r'^30\|\t\t\}$', r'^31\|\t\}$', r'^32\|\tvirtual void\tVent\(void\) =0;$',
		 r'^1\|#include "Tank\.h"$', r'^3\|$', r'^4\|/{74}$', r'^8\|void CTank::Init\(int iSize\)$', r'^12\|\}$', r'^13\|$', r'^14\|$', r'^15\|/{74}$', r'^19\|int CountTanks\(CTank\* pacTanks, int iCount\)$'],
		[r'^Definitions: refused', r'Whether it holds', r'CTank::CTank\(int', r'CTank::Empty\(']),
	('definitions-move-in-template', dict(MakeDefinitionsCopy(), **{
		#A class template's member comes in from under its template<...>, which goes with it; an empty body is {}.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '37:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movedefinitionin,printdisklines:Tank.h'}),
		[r"^Status: Moved CStore::Init's definition into its class, from Tank\.h\.$",
		 r'^37\|\tvoid\tInit\(void\) \{\}$', r'^40\|\};$', r'^41\|$', r'^42\|$', r'^43\|int\t\tMeasureTank\(CTank\* pcTank\);$', r'^46 lines$'],
		[r'^Definitions: refused', r'CStore<M>::Init']),
	('definitions-generate-declaration', dict(MakeDefinitionsCopy(), **{
		#Generate Declaration: a function defined in a source file is declared in its header, after the functions it
		#declares.  A member defined that its class doesn't declare (which is nothing to the compiler: the file says
		#what it is) is declared in the class, after its last function and lined up with it.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:52:5,edit:generatedeclaration,edit:caret:62:14,edit:generatedeclaration,printdisklines:Tank.h'}),
		[r'^Status: Declared CountTanks in Tank\.h\.$', r'^Status: Declared CTank::Reset in Tank\.h\.$',
		 r'^26\|\tvirtual void\tVent\(void\) =0;$', r'^27\|\tvoid\t\t\tReset\(int iLevel\);$', r'^28\|\};$',
		 r'^50\|int\t\tMeasureTank\(CTank\* pcTank\);$', r'^51\|int\t\tCountTanks\(CTank\* pacTanks, int iCount\);$', r'^52\|$', r'^54\|#endif // __TANK_H__$'],
		[r'^Definitions: refused']),
	('definitions-refused', dict(MakeDefinitionsCopy(), **{
		#What can't be done, and why: nothing is changed.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:26:18,edit:generatedefinition,edit:caret:13:8,edit:generatedefinition,edit:caret:24:8,edit:movedefinitionout,'
		         'edit:caret:15:8,edit:movedefinitionin,edit:caret:8:7,edit:generatedefinition,edit:caret:14:8,edit:generatedeclaration,edit:caret:14:8,edit:movedefinitionin,'
		         'edit:caret:3:1,edit:generatedeclaration,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r"^Status: Can't generate a definition of CTank::Vent: it is pure virtual\.$", r"^Status: Can't generate a definition of CTank::Init: it is defined already\.$",
		 r"^Status: Can't move the definition of CTank::IsFull: it isn't defined in its class\.$", r"^Status: Can't move the definition of CTank::GetLevel: it is defined in its class already\.$",
		 r"^Status: Can't generate a definition: it isn't a function\.$", r"^Status: Can't generate a declaration of CTank::Fill: it is declared already\.$",
		 r"^Status: Can't move the definition of CTank::Fill: it has no definition\.$", r"^Status: Can't generate a declaration: there is no function at the caret\.$",
		 r'^52 lines$', r'^65 lines$'],
		[r'^Definitions: done']),
	('definitions-undo', dict(MakeDefinitionsCopy(), **{
		#One Undo Across Files takes it back in both files (the first press says where: the other file isn't in view).
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '16:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movedefinitionout,edit:undoall,edit:undoall,edit:saveall,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r"^Status: Moved CTank::Drain's definition out of its class, to Tank\.cpp\.$", r'^Status: Ctrl\+Z again to undo the change in Tank\.cpp', r'^16\|\tint\t\tDrain\(int iAmount\)$', r'^23\|\t\}$', r'^52 lines$', r'^65 lines$'],
		[r'CTank::Drain\(']),
	('definitions-function-in', dict(MakeDefinitionsCopy(), **{
		#Move Definition into Class for a function outside any class: a header declares it and a source file defines
		#it, so it is defined in the header, inline, as the source file wrote it and last in the header (what its body
		#names is declared by there); it stays declared where it was, with its default value.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movedefinitionin,printdisklines:Gauge.h,printdisklines:Gauge.cpp'}),
		[r"^Status: Moved ReadGauge's definition into its header, Gauge\.h, from Gauge\.cpp\.$",
		 r'^5\|int\t\tReadGauge\(int iGauge, int iScale = 1\);$', r'^13\|inline int ClampGauge\(int iValue, int iMax = 100\)$', r'^24\|M MaxGauge\(M a, M b\)$',
		 r'^27\|\}$', r'^28\|$', r'^29\|$', r'^30\|/{74}$', r'^33\|/{74}$', r'^34\|inline int ReadGauge\(int iIndex, int iScale\)$', r'^35\|\{$', r'^36\|\treturn ClampGauge\(iIndex \* iScale, 100\);$', r'^37\|\}$',
		 r'^38\|$', r'^39\|$', r'^40\|#endif // __GAUGE_H__$',
		 r'^1\|#include "Gauge\.h"$', r'^3\|$', r'^4\|/{74}$', r'^8\|int ResetGauge\(int iGauge\)$', r'^11 lines$'],
		[r'^Definitions: refused']),
	('definitions-function-out', dict(MakeDefinitionsCopy(), **{
		#Move Definition out of Class for a function outside any class that a header defines: it is defined in the
		#header's source file, after the functions declared before it, without its inline or its default value, and
		#declared where it was.  A function template stays in its header.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:24:4,edit:movedefinitionout,edit:caret:13:14,edit:movedefinitionout,printdisklines:Gauge.h,printdisklines:Gauge.cpp'}),
		[r"^Status: Can't move the definition of MaxGauge: a function template is defined in its header\.$", r"^Status: Moved ClampGauge's definition out of its header, to Gauge\.cpp\.$",
		 r'^6\|int\t\tResetGauge\(int iGauge\);$', r'^7\|$', r'^8\|$', r'^9\|int ClampGauge\(int iValue, int iMax = 100\);$', r'^10\|$', r'^11\|$', r'^12\|/{74}$', r'^16\|template<class M>$', r'^23 lines$',
		 r'^18\|int ResetGauge\(int iGauge\)$', r'^21\|\}$', r'^22\|$', r'^23\|$', r'^24\|/{74}$', r'^28\|int ClampGauge\(int iValue, int iMax\)$', r'^29\|\{$',
		 r'^30\|\treturn \(iValue > iMax\) \? iMax : iValue;$', r'^31\|\}$', r'^31 lines$'],
		[r'inline int ClampGauge']),
	('definitions-match-declaration', dict(MakeDefinitionsCopy(), **{
		#A member's definition was edited (what it returns, its parameters' names, no const), so its class declares
		#one nothing defines: Generate Declaration on the definition changes that declaration to match.  It keeps its
		#virtual, its default value (the parameter after which every one has one) and its name's column.
		'WAIT_INDEX': '1', 'OPEN': 'Valve.cpp', 'CARET': '18:15',
		'DEBUG': 'edit:renamesearch:---s,edit:generatedeclaration,printdisklines:Valve.h,printdisklines:Valve.cpp'}),
		[r"^Status: Changed CValve::Open's declaration in Valve\.h to match its definition\.$",
		 r'^11\|\tvoid\t\t\tInit\(void\);$', r'^12\|\tvirtual long\tOpen\(int iFlow, int iTime = 5\);$', r'^13\|\tvoid\t\t\tClose\(int iDelay\);$', r'^18 lines$',
		 r'^18\|long CValve::Open\(int iFlow, int iTime\)$', r'^41 lines$'],
		[r'^Definitions: refused']),
	('definitions-match-definition', dict(MakeDefinitionsCopy(), **{
		#A member's declaration was edited: Generate Definition on it changes the definition it had to match, and
		#makes no second one.  A parameter whose type is as it was keeps the name the body uses (iDegrees).
		'WAIT_INDEX': '1', 'OPEN': 'Valve.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:14:10,edit:generatedefinition,edit:open:Valve.h,edit:caret:13:10,edit:generatedefinition,'
		         'printdisklines:Valve.h,printdisklines:Valve.cpp'}),
		[r"^Status: Changed CValve::Turn's definition in Valve\.cpp to match its declaration\.$", r"^Status: Changed CValve::Close's definition in Valve\.cpp to match its declaration\.$",
		 r'^13\|\tvoid\t\t\tClose\(int iDelay\);$', r'^14\|\tint\t\t\t\tTurn\(int iAngle, bool bFast\) const;$', r'^18 lines$',
		 r'^18\|long CValve::Open\(int iFlow, int iTime\)$', r'^28\|void CValve::Close\(int iDelay\)$', r'^38\|int CValve::Turn\(int iDegrees, bool bFast\) const$', r'^40\|\treturn iDegrees \+ miFlow;$', r'^41 lines$'],
		[r'^Definitions: refused']),
	('convert-member-assembly', dict(MakeMoveAssemblyCopy(), **{
		#Convert to Function, and back to a member, where hand written assembly calls the function by the compiler's
		#label: the label has the function's class and its parameters in it, so the compiler is asked for it as it
		#is and as it becomes.
		'WAIT_INDEX': '1', 'OPEN': 'Tun.cpp', 'CARET': '58:13',
		'DEBUG': 'edit:renamesearch:---s,edit:membertofunction,printdisklines:TunW65C816.asm,edit:open:Tun.cpp,edit:caret:58:7,edit:functiontomember,printdisklines:TunW65C816.asm'}),
		[r'^Status: Converted CTun::Stir to the function Stir\(CTun\* pcTun, \.\.\.\): 0 calls changed, 1 label in assembly\.$',
		 r'^7\|\t\tjsr _CTun__Fill$', r'^8\|\t\tjsr _Stir__FPC4CTun$', r'^9\|\t\tjsr _CTun__Drain__Fi$',
		 r'^Status: Converted Stir .*1 label in assembly\.$', r'^8\|\t\tjsr _CTun__Stir$'],
		[]),
	('extract-superclass-assembly', dict(MakeMoveAssemblyCopy(), **{
		#Extract Superclass where hand written assembly names the members that go: their labels get the new class.
		'WAIT_INDEX': '1', 'OPEN': 'Tun.h', 'CARET': '14:7',
		'DEBUG': 'edit:renamesearch:---s,edit:extractsuperclass:CKettle|miLevel;Fill();Stir(),printdisklines:TunW65C816.asm'}),
		[r'^Status: Extracted the superclass CKettle from CTun: 3 members, in Kettle\.h and Kettle\.cpp; 4 labels in assembly changed\.$',
		 r'^6\|\t\tlda CKettle_miLevel,x$', r'^7\|\t\tjsr _CKettle__Fill$', r'^8\|\t\tjsr _CKettle__Stir$', r'^9\|\t\tjsr _CTun__Drain__Fi$', r'^12\|\t\tjsr _CVat__Init$', r'^13\|\t\tjsr _CKettle__Fill$'],
		[]),
	('extract-template-assembly', dict(MakeTemplateAssemblyCopy(), **{
		#Extract Template Parameter from a class whose functions hand written assembly calls: they are the functions
		#of the template's instance afterwards, and their labels have its argument in them.
		#Remove Template Parameter takes the argument out of the labels again, and they follow the class back to the
		#name it had.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '9:3',
		'DEBUG': 'edit:renamesearch:---s,edit:extracttemplate:M:CTankTemplate,printdisklines:TankW65C816.asm,edit:caret:6:17,edit:removetemplateparameter,printdisklines:TankW65C816.asm,printdisklines:Tank.h'}),
		[r'^Status: Extracted the template parameter M from CGauge in CTank: .*; 2 labels in assembly changed\.$',
		 r'^6\|\t\tjsr _CTankTemplate__T_C6CGauge__Init__FPC6CGauge$', r'^7\|\t\tjsr _CTankTemplate__T_C6CGauge__Level$', r'^8\|\t\tjsr _CGauge__Read$',
		 r'^Status: Removed the template parameter M from CTankTemplate: .*  2 labels in assembly changed\.  It is CTank again, which was the class that was its name\.$',
		 r'^6\|\t\tjsr _CTank__Init__FPC6CGauge$', r'^7\|\t\tjsr _CTank__Level$', r'^8\|\t\tjsr _CGauge__Read$', r'^6\|class CTank$'],
		[]),
	('pull-up-assembly', dict(MakeMoveAssemblyCopy(), **{
		#Pull Members Up where hand written assembly names the members by the compiler's labels: a field's layout
		#constant, a function's label (twice) and an overload's that is told by its parameters get the class they
		#went to.  The overload whose label fits another overload too (an int's and a long's are written alike on
		#one processor or the other) is left, and said; what stayed in the class keeps its labels.
		'WAIT_INDEX': '1', 'OPEN': 'Tun.h', 'CARET': '14:7',
		'DEBUG': 'edit:renamesearch:---s,edit:pullup:CVat|miLevel;Fill();Drain(int);Drain(char),printdisklines:TunW65C816.asm,printdisklines:Tun.h,edit:undoall,edit:undoall,edit:saveall,printdisklines:TunW65C816.asm'}),
		[r"^Status: Pulled 4 members up from CTun to CVat; 4 labels in assembly changed; 1 label that may be an overload's left\.$",
		 r'^6\|\t\tlda CVat_miLevel,x$', r'^7\|\t\tjsr _CVat__Fill$', r'^8\|\t\tjsr _CTun__Stir$', r'^9\|\t\tjsr _CTun__Drain__Fi$', r'^10\|\t\tjsr _CTun__Drain__Fx$',
		 r'^11\|\t\tjsr _CVat__Drain__Fc$', r'^12\|\t\tjsr _CVat__Init$', r'^13\|\t\tjsr _CVat__Fill$', r'^14 lines$',
		 r'^12\|\tvoid\tFill\(void\);$', r'^6\|\t\tlda CTun_miLevel,x$', r'^11\|\t\tjsr _CTun__Drain__Fc$'],
		[]),
	('move-members-pull-up', dict(MakeMoveMembersCopy(), **{
		#Pull Members Up: what is found for the class the caret is in (its members, their access, where each is
		#defined, and its base classes), and the member at the caret moved: its declaration to the end of the base
		#class's public part, lined up with the declarations there, and its definition to the base class's source
		#file under the base class's name.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:pullable,edit:pullup:CVessel|Spare(),printdisklines:Vessel.h,printdisklines:Drum.h,printdisklines:Vessel.cpp,printdisklines:Drum.cpp'}),
		[r'^Pull Members Up: CDrum to CVessel$', r'^  private field miSecret$', r'^  protected field miLevel$',
		 r"^  protected field miRate, can't be moved: it is declared with miStep in one declaration$", r'^  public function Init\(int\), defined in Drum\.cpp$',
		 r'^  public function GetLevel\(\)$', r'^  public function Spare\(\), defined in Drum\.cpp, chosen$',
		 r'^Status: Pulled Spare\(\) up from CDrum to CVessel\.$', r'^Move members: moved$',
		 r'^14\|\tvoid\t\t\tSeal\(void\);$', r'^15\|\tint\t\t\t\tSpare\(void\);$', r'^16\|\};$',
		 r'^19\|\tvoid\tFill\(int iAmount\);$', r'^20\|\tvoid\tRoll\(void\);$',
		 r'^31\|\}$', r'^32\|$', r'^33\|$', r'^34\|/{74}$', r'^38\|int CVessel::Spare\(void\)$', r'^40\|\treturn miVolume - 1;$', r'^41 lines$',
		 r'^19\|void CDrum::Fill\(int iAmount\)$', r'^22\|\}$', r'^23\|$', r'^24\|$', r'^25\|/{74}$', r'^29\|void CDrum::Roll\(void\)$', r'^72 lines$'],
		[r'^Conflict', r'CDrum::Spare']),
	('move-members-pull-up-field', dict(MakeMoveMembersCopy(), **{
		#A field pulled up goes after the base class's last field of its access, not after the functions at the end
		#of the part.
		'WAIT_INDEX': '1', 'OPEN': 'Rack.h', 'CARET': '18:8',
		'DEBUG': 'edit:renamesearch:---s,edit:pullup:CRackBase|miDepth,printdisklines:Rack.h'}),
		[r'^Status: Pulled miDepth up from CRack to CRackBase\.$', r'^8\|\tint\t\tmiSlots;$', r'^9\|\tint\t\tmiDepth;$', r'^10\|$', r'^11\|\tvoid\tInit\(void\) \{ miSlots = 0; \}$',
		 r'^16\|class CRack : public CRackBase$', r'^18\|public:$', r'^19\|\tint\t\tDepth\(void\) \{ return miDepth; \}$', r'^23 lines$'],
		[]),
	('move-members-pull-up-several', dict(MakeMoveMembersCopy(), **{
		#Several at once: a field to the base class's protected part, a function defined in the class with the comment
		#over it, and a static function with its definition.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:pullup:CVessel|miLevel;GetLevel();Count(),printdisklines:Vessel.h,printdisklines:Drum.h,printdisklines:Vessel.cpp'}),
		[r'^Status: Pulled 3 members up from CDrum to CVessel\.$',
		 r'^8\|\tint\t\tmiVolume;$', r'^9\|\tint\t\tmiLevel;$', r'^10\|$', r'^11\|public:$', r'^15\|\tvoid\t\t\tSeal\(void\);$', r'^16\|\t//How full it is\.$',
		 r'^17\|\tint\t\t\t\tGetLevel\(void\) \{ return miLevel; \}$', r'^18\|\tstatic int\t\tCount\(void\);$', r'^19\|\};$',
		 r'^11\|protected:$', r'^12\|\tint\t\tmiRate, miStep;$', r'^15\|\tvoid\tInit\(int iVolume\);$', r'^16\|\tvoid\tFill\(int iAmount\);$', r'^19\|\tvoid\tHide\(void\);$',
		 r'^38\|int CVessel::Count\(void\)$'],
		[r'^Conflict']),
	('move-members-pull-up-conflicts', dict(MakeMoveMembersCopy(), **{
		#What stands in the way: a member that uses one the base class won't have; a private one the class's others
		#use; one declared with another; one the base class has already.  Nothing is moved.  Then a function pulled up
		#abstract: the base class gets it pure virtual and the class keeps its own.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:pullup:CVessel|Fill(int),edit:pullup:CVessel|miSecret,edit:pullup:CVessel|miRate,edit:pullup:CVessel|Init(int),edit:pullup:CVessel|Nope,'
		         'edit:pullup:CVessel|miLevel|Count(),edit:pullup:CVessel|Fill(int);miLevel,edit:undoall,edit:undoall,edit:pullup:CVessel||Roll(),printdisklines:Vessel.h,printdisklines:Drum.h'}),
		[r"^Conflict: Fill\(int\) uses CDrum::miLevel, which CVessel doesn't have\.$", r'^Move members: not moved$',
		 r'^Conflict: miSecret will be private in CVessel: CDrum::Hide uses it\.$', r"^Conflict: miRate can't be moved: it is declared with miStep in one declaration\.$",
		 r'^Conflict: CVessel already has Init\(int\)\.$', r'^Move members: there is no Nope$',
		 r"^Conflict: Count\(\) can't be abstract: only a function that isn't static or a template can be\.$",
		 r'^Status: Pulled 2 members up from CDrum to CVessel\.$', r'^Status: Pulled Roll\(\) up from CDrum to CVessel\.$',
		 r'^14\|\tvoid\t\t\tSeal\(void\);$', r'^15\|\tvirtual void\tRoll\(void\) =0;$', r'^16\|\};$', r'^12\|\tint\t\tmiLevel;$', r'^21\|\tvoid\tRoll\(void\) override;$', r'^35 lines$'],
		[]),
	('move-members-push-down', dict(MakeMoveMembersCopy(), **{
		#Push Members Down: the classes that derive from the class each get the member and a copy of its definition
		#under their own name, in the file their functions are defined in.  A use through the class itself stands in
		#the way (pcVessel->Seal(), not cDrum.Seal(): the object's type is asked of the compiler); Continue pushes
		#it down all the same.
		'WAIT_INDEX': '1', 'OPEN': 'Vessel.h', 'CARET': '14:10',
		'DEBUG': 'edit:renamesearch:---s,edit:pushable,edit:pushdown:*|Seal(),edit:pushdownanyway:*|Seal(),'
		         'printdisklines:Vessel.h,printdisklines:Drum.h,printdisklines:Vessel.cpp,printdisklines:Drum.cpp'}),
		[r'^Push Members Down: CVessel to CDrum, CKeg$', r'^  public function Seal\(\), defined in Vessel\.cpp, chosen$',
		 r"^Conflict: main uses Seal\(\) \(Main\.cpp line 20\), which CVessel won't have\.$", r'^Move members: not moved$',
		 r'^Status: Pushed Seal\(\) down from CVessel to CDrum, CKeg\.$',
		 r'^13\|\tvirtual void\tVent\(void\);$', r'^14\|\};$',
		 r'^23\|\tvoid\tHide\(void\);$', r'^24\|\tvoid\tSeal\(void\);$', r'^25\|\};$', r'^32\|\tvoid\tVent\(void\) override;$', r'^33\|\tvoid\tSeal\(void\);$', r'^34\|\};$',
		 r'^18\|void CVessel::Vent\(void\)$', r'^21 lines$',
		 r'^79\|void CKeg::Vent\(void\)$', r'^82\|\}$', r'^83\|$', r'^84\|$', r'^85\|/{74}$', r'^89\|void CDrum::Seal\(void\)$', r'^91\|\tmiVolume\+\+;$', r'^92\|\}$', r'^93\|$', r'^94\|$', r'^95\|/{74}$',
		 r'^99\|void CKeg::Seal\(void\)$', r'^102 lines$'],
		[r'Main\.cpp line 18', r'CVessel::Seal']),
	('move-members-push-down-abstract', dict(MakeMoveMembersCopy(), **{
		#Kept abstract: the class keeps the function pure virtual, so nothing that uses it is in the way; a class that
		#overrides it already keeps its own.  A field that the class's own functions use can't go.
		'WAIT_INDEX': '1', 'OPEN': 'Vessel.h', 'CARET': '14:10',
		'DEBUG': 'edit:renamesearch:---s,edit:pushdown:*||Vent(),edit:pushdown:CDrum|miVolume,printdisklines:Vessel.h,printdisklines:Drum.h,printdisklines:Vessel.cpp,printdisklines:Drum.cpp'}),
		[r'^Status: Pushed Vent\(\) down from CVessel to CDrum, CKeg\.$',
		 r"^Conflict: CKeg::Tap uses miVolume \(Drum\.cpp line 71\), which CVessel won't have\.$", r"^Conflict: CVessel::Init uses miVolume \(Vessel\.cpp line 10\), which CVessel won't have\.$", r'^Move members: not moved$',
		 r'^8\|\tint\t\tmiVolume;$', r'^13\|\tvirtual void\tVent\(void\) =0;$', r'^14\|\tvoid\t\t\tSeal\(void\);$',
		 r'^23\|\tvoid\tHide\(void\);$', r'^24\|\tvirtual void\tVent\(void\) override;$', r'^25\|\};$', r'^31\|\tvoid\tTap\(void\);$', r'^32\|\tvoid\tVent\(void\) override;$', r'^33\|\};$',
		 r'^18\|void CVessel::Seal\(void\)$', r'^21 lines$', r'^79\|void CKeg::Vent\(void\)$', r'^89\|void CDrum::Vent\(void\)$', r'^91\|\tmiVolume = 0;$', r'^92 lines$'],
		[r'CVessel::Vent', r'CDrum::Spare uses']),
	('move-members-template', dict(MakeMoveMembersCopy(), **{
		#A class template's member defined outside it goes to the class template it derives from with its body in the
		#class: it needs no template<...> and no class's name there.
		'WAIT_INDEX': '1', 'OPEN': 'Crate.h', 'CARET': '19:8',
		'DEBUG': 'edit:renamesearch:---s,edit:pullable,edit:pullup:CBox|Size(),printdisklines:Crate.h'}),
		[r'^Pull Members Up: CCrate to CBox \(template\)$', r'^  public function Size\(\), defined in Crate\.h, chosen$', r'^Status: Pulled Size\(\) up from CCrate to CBox\.$',
		 r'^11\|\tM\*\t\tGet\(void\) \{ return &mcItem; \}$', r'^12\|\tint\t\tSize\(void\) \{ return sizeof\(M\); \}$', r'^13\|\};$', r'^17\|class CCrate : public CBox<M>$', r'^23 lines$'],
		[r'CCrate<M>::Size']),
	('move-members-refused', dict(MakeMoveMembersCopy(), **{
		#A class with nowhere for its members to go, and no class at the caret.
		'WAIT_INDEX': '1', 'OPEN': 'Vessel.h', 'CARET': '14:10',
		'DEBUG': 'edit:renamesearch:---s,edit:pullable,edit:open:Drum.h,edit:caret:20:8,edit:pushable,edit:caret:3:3,edit:pullable'}),
		[r"^Pull Members Up: CVessel can't be: it has no base class in the solution$", r"^Push Members Down: CDrum can't be: no class in the solution derives from it$", r'^Pull Members Up: no class$'],
		[]),
	('move-members-dialog', dict(MakeMoveMembersCopy(), **{
		#The dialogs: the member at the caret is the one ticked, and Refactor moves it; what stands in the way is shown
		#in Problems Detected, where Cancel moves nothing.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|OK|Cancel',
		'DEBUG': 'pullmembersup,answered:1,printstatus,edit:open:Drum.h,edit:caret:19:8,pullmembersup,answered:3,printstatus'}),
		[r'^Dialog \[Pull Members Up from CDrum\] answered OK$', r'^Status: Pulled Spare\(\) up from CDrum to CVessel\.$',
		 r'^Dialog \[Pull Members Up from CDrum\] answered OK$', r'^Dialog \[Problems Detected\] answered Cancel$'],
		[r'Pulled Fill']),
	('move-namespace-class', dict(MakeNamespacesCopy(), **{
		#Move to Namespace (Refactorings.txt 21): CPoint from Geometry to a new namespace Shapes, alone (Area, which takes
		#it, not ticked to go with it).  Problems Detected says Geometry's Area takes it (argument dependent lookup).  Its declaration and its members' definitions in
		#Geometry's blocks go into Shapes blocks where they are (Geometry's closed before and opened after); what they
		#name of Geometry is qualified; every use names Shapes::CPoint.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:10:9,edit:movetonamespacealone:Shapes,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace: Geometry::CPoint from \[Geometry\], 3 declarations: Shapes\.h\[1\] Shapes\.cpp\[1\] Shapes\.cpp\[1\], taken by: Geometry::Area\(Geometry::CPoint\*\)$', r'^Dialog \[Problems Detected\] answered OK$',
		 r'^Move to Namespace status: Moved Geometry::CPoint to Shapes: 8 uses; 4 labels in assembly changed\.$',
		 r'^7\|\ttypedef int\tTLength;$', r'^8\|\}$', r'^11\|namespace Shapes$', r'^12\|\{$', r'^13\|\tclass CPoint$', r'^16\|\t\tGeometry::TLength\tmiX;$',
		 r'^19\|\t\tvoid\tInit\(Geometry::TLength iX, Geometry::TLength iY\);$', r'^21\|\t\};$', r'^22\|\}$', r'^25\|namespace Geometry$', r'^27\|\tTLength\tArea\(Shapes::CPoint\* pcPoint\);$',
		 r'^\d+\|\tvoid\tPlot\(Shapes::CPoint\* pcPoint\);$',
		 r'^6\|\tint\tgiCount = 1;$', r'^7\|\}$', r'^10\|namespace Shapes$', r'^16\|\tvoid CPoint::Init\(Geometry::TLength iX, Geometry::TLength iY\)$', r'^19\|\t\tmiY = Geometry::Detail::Twice\(iY\);$',
		 r'^27\|\tGeometry::TLength CPoint::Sum\(void\)$', r'^31\|\}$', r'^38\|Geometry::TLength Geometry::Area\(Shapes::CPoint\* pcPoint\)$', r'^58\|void Drawing::Plot\(Shapes::CPoint\* pcPoint\)$',
		 r'^14\|\tShapes::CPoint\t\t\t\tcPoint;$', r'^15\|\tShapes::CPoint\tcOther;$',
		 r'^5\|_Shapes__CPoint__Sum:$', r'^7\|_Geometry__Area__FPC14Shapes__CPoint:$', r'^8\|\t\tjsr _Shapes__CPoint__Sum$', r'^13\|_Drawing__Plot__FPC14Shapes__CPoint:$'],
		[r'^\d+\|.*Geometry::CPoint']),
	('move-namespace-class-with', dict(MakeNamespacesCopy(), **{
		#Move to Namespace of a class offers its old namespace's functions that take it, ticked, to go with it
		#(question 63): Area goes to Shapes with CPoint, into the same block, its definition Shapes::Area and its call
		#Shapes::Area, and both their labels change; nothing that takes CPoint stays, so there is no warning.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:10:9,edit:movetonamespace:Shapes,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace: Geometry::CPoint from \[Geometry\], 3 declarations: Shapes\.h\[1\] Shapes\.cpp\[1\] Shapes\.cpp\[1\], taken by: Geometry::Area\(Geometry::CPoint\*\)$',
		 r'^Move to Namespace status: Moved Geometry::CPoint and Geometry::Area to Shapes: 9 uses; 5 labels in assembly changed\.$',
		 r'^8\|\}$', r'^11\|namespace Shapes$', r'^13\|\tclass CPoint$', r'^21\|\t\};$', r'^24\|\tGeometry::TLength\tArea\(CPoint\* pcPoint\);$', r'^25\|\}$', r'^28\|namespace Geometry$', r'^30\|\textern int\tgiCount;$',
		 r'^38\|Geometry::TLength Shapes::Area\(Shapes::CPoint\* pcPoint\)$', r'^40\|\treturn pcPoint->Sum\(\) \* Geometry::giCount;$',
		 r'^21\|\ti = Shapes::Area\(&cOther\) \+ Geometry::Detail::Twice\(giCount\);$',
		 r'^7\|_Shapes__Area__FPC14Shapes__CPoint:$', r'^13\|_Drawing__Plot__FPC14Shapes__CPoint:$'],
		[r'Problems Detected', r'^\d+\|.*Geometry::Area']),
	('move-namespace-class-with-dialog', dict(MakeNamespacesCopy(), **{
		#The dialog lists Area under "Functions that take it", ticked; space unticks it, and it stays (Problems Detected
		#says so).
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '10:9', 'DIALOG_KEYS': 'type:Shapes,tab,tab,space|', 'DIALOG_ANSWER': 'OK|OK',
		'DEBUG': 'edit:renamesearch:---s,command:Refactor.MoveToNamespace,answered:2,printstatus'}),
		[r'^Dialog \[Move Geometry::CPoint to Namespace\] answered OK$', r'^Dialog \[Problems Detected\] answered OK$', r'^Status: Moved Geometry::CPoint to Shapes: 8 uses; 4 labels in assembly changed\.$'],
		[]),
	('move-namespace-function', dict(MakeNamespacesCopy(), **{
		#Area from Geometry to Shapes: its declaration in a Shapes block where it was, its definition outside the
		#namespaces renamed Shapes::Area with what it names of Geometry qualified, and Main.cpp's call (found through
		#its using-directive of Geometry) Shapes::Area.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:21:10,edit:movetonamespace:Shapes,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace: Geometry::Area\(Geometry::CPoint\*\) from \[Geometry\], 2 declarations: Shapes\.h\[1\] Shapes\.cpp\(qualified\)\[0\]$',
		 r'^Move to Namespace status: Moved Geometry::Area to Shapes: 1 use; 1 label in assembly changed\.$', r'^18\|\t\};$', r'^19\|\}$', r'^22\|namespace Shapes$', r'^24\|\tGeometry::TLength\tArea\(Geometry::CPoint\* pcPoint\);$',
		 r'^25\|\}$', r'^28\|namespace Geometry$', r'^30\|\textern int\tgiCount;$', r'^35\|Geometry::TLength Shapes::Area\(Geometry::CPoint\* pcPoint\)$', r'^37\|\treturn pcPoint->Sum\(\) \* Geometry::giCount;$',
		 r'^21\|\ti = Shapes::Area\(&cOther\) \+ Geometry::Detail::Twice\(giCount\);$', r'^7\|_Shapes__Area__FPC16Geometry__CPoint:$', r'^13\|_Drawing__Plot__FPC16Geometry__CPoint:$'],
		[r'Problems Detected']),
	('move-namespace-global', dict(MakeNamespacesCopy(), **{
		#Geometry::Detail::Twice to the global namespace: Detail's block, which it was alone in, goes, and it follows
		#Geometry's block; its definition is Twice, and its uses lose Detail:: and Geometry::Detail::.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:26:7,edit:movetonamespace:,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace status: Moved Geometry::Detail::Twice to the global namespace: 2 uses; 2 labels in assembly changed\.$', r'^22\|\textern int\tgiCount;$', r'^23\|\}$', r'^26\|int\tTwice\(int i\);$', r'^29\|namespace Drawing$',
		 r'^16\|\t\tmiY = Twice\(iY\);$', r'^45\|int Twice\(int i\)$', r'^21\|\ti = Area\(&cOther\) \+ Twice\(giCount\);$', r'^11\|_Twice__Fi:$', r'^14\|\t\tjsr _Twice__Fi$'],
		[r'^\d+\|.*namespace Detail', r'^\d+\|.*Detail::']),
	('move-namespace-nested', dict(MakeNamespacesCopy(), **{
		#giCount from Geometry to Geometry::Detail: it goes into the Detail block next to it, and Main.cpp (with its
		#using-directive of Geometry) names it Detail::giCount.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:22:13,edit:movetonamespace:Geometry::Detail,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace status: Moved Geometry::giCount to Geometry::Detail: 2 uses; 1 label in assembly changed\.$', r'^21\|\tTLength\tArea\(CPoint\* pcPoint\);$', r'^22\|$', r'^23\|\tnamespace Detail$', r'^25\|\t\textern int\tgiCount;$',
		 r'^26\|$', r'^27\|\t\tint\tTwice\(int i\);$', r'^\d+\|\treturn pcPoint->Sum\(\) \* Detail::giCount;$', r'^21\|\ti = Area\(&cOther\) \+ Geometry::Detail::Twice\(Detail::giCount\);$', r'^9\|\t\tlda _Geometry__Detail__giCount$'],
		[r'^\d+\|\tnamespace Detail\n.*\n.*\n\tnamespace Detail']),
	('move-namespace-usings', dict(MakeNamespacesCopy(), **{
		#With "Add using-declarations": Main.cpp gets using Shapes::Area; with its other using lines and its call is
		#left as it is written.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:21:10,edit:movetonamespaceusings:Shapes,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace status: Moved Geometry::Area to Shapes: 1 use, with using-declarations; 1 label in assembly changed\.$', r'^4\|using namespace Geometry;$', r'^5\|using Drawing::Plot;$', r'^6\|using Shapes::Area;$',
		 r'^\d+\|\ti = Area\(&cOther\) \+ Geometry::Detail::Twice\(giCount\);$'],
		[r'Shapes::Area\(&cOther']),
	('move-namespace-c-linkage', dict(MakeNamespacesCopy(), **{
		#A function declared inside extern "C" is labelled by its name alone: moving it changes no label.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:37:18,edit:movetonamespace:Shapes,printdisklines:Shapes.h,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace status: Moved Drawing::Clear to Shapes: 0 uses\.$', r'^\d+\|namespace Shapes$', r'^\d+\|\textern "C" void\tClear\(void\);$', r'^16\|_Clear:$'],
		[r'_Shapes__Clear']),
	('move-namespace-conflict', dict(MakeNamespacesCopy(), **{
		#Geometry::TLength to Drawing, which has a TLength: Problems Detected, and Cancel moves nothing.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:7:15,edit:movetonamespace:Drawing,printdisklines:Shapes.h'}),
		[r'^Dialog \[Problems Detected\] answered Cancel$', r'^Move to Namespace status: Move to Namespace: nothing was moved\.$', r'^7\|\ttypedef int\tTLength;$'],
		[r'Moved']),
	('move-namespace-dialog', dict(MakeNamespacesCopy(), **{
		#Refactor, Move to Namespace...: its dialog has the namespace it is in, selected, to type over.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '21:10', 'DIALOG_KEYS': 'type:Shapes', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:renamesearch:---s,command:Refactor.MoveToNamespace,answered:1,printstatus,edit:saveall,printdisklines:Main.cpp'}),
		[r'^Dialog \[Move Geometry::Area to Namespace\] answered OK$', r'^Status: Moved Geometry::Area to Shapes: 1 use; 1 label in assembly changed\.$', r'^21\|\ti = Shapes::Area\(&cOther\)'],
		[]),
	('move-namespace-completion', dict(MakeNamespacesCopy(), **{
		#The dialog's field completes over the solution's namespaces as it is typed in; down and enter take one.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '21:10', 'DIALOG_KEYS': 'type:Det,down,enter', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:renamesearch:---s,command:Refactor.MoveToNamespace,answered:1,printstatus'}),
		[r'^Field \[Det\] completes: Geometry::Detail$', r'^Field \[Det\] completes: \*Geometry::Detail$', r'^Field \[Geometry::Detail\]$',
		 r'^Status: Moved Geometry::Area to Geometry::Detail: 1 use; 1 label in assembly changed\.$'],
		[]),
	('move-function-namespace', dict(MakeNamespacesCopy(), **{
		#Move to File of a function in a namespace (Refactorings.txt 21 g): Area's prototype goes into Drawing.h's
		#Geometry block, after what is in it; its definition, written with its namespace, goes to the end of
		#Drawing.cpp as it is.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '21:10',
		'DEBUG': 'edit:renamesearch:---s,edit:movablefunction,edit:movefunction:Drawing.h,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Drawing.h,printdisklines:Drawing.cpp'}),
		[r'^Move to File: Geometry::Area\(Geometry::CPoint\*\), declared in Shapes\.h, defined in Shapes\.cpp, 1 file names it$', r'^Move to File: moved$',
		 r'^Disk lines Shapes\.h:$', r'^18\|\t\};$', r'^19\|$', r'^20\|$', r'^21\|\textern int\tgiCount;$',
		 r'^Disk lines Drawing\.h:$', r'^6\|namespace Geometry$', r'^8\|\tint\tHalf\(int i\);$', r'^9\|$', r'^10\|\tTLength\tArea\(CPoint\* pcPoint\);$', r'^11\|\}$', r'^14\|#endif',
		 r'^Disk lines Drawing\.cpp:$', r'^15\|\}$', r'^22\|Geometry::TLength Geometry::Area\(CPoint\* pcPoint\)$', r'^25 lines$'],
		[]),
	('move-function-namespace-nested', dict(MakeNamespacesCopy(), **{
		#Geometry::Detail::Twice to Drawing.h, which has a Geometry block and no Detail block: a Detail block in the
		#Geometry one; the Detail block it leaves empty in Shapes.h goes, with the blank line before it.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '26:7',
		'DEBUG': 'edit:renamesearch:---s,edit:movablefunction,edit:movefunction:Drawing.h,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Drawing.h,printdisklines:Drawing.cpp'}),
		[r'^Move to File: moved$', r'^Disk lines Shapes\.h:$', r'^22\|\textern int\tgiCount;$', r'^23\|\}$', r'^24\|$', r'^25\|$', r'^26\|namespace Drawing$',
		 r'^Disk lines Drawing\.h:$', r'^7\|\tint\tHalf\(int i\);$', r'^8\|$', r'^9\|\tnamespace Detail$', r'^10\|\t\{$', r'^11\|\t\tint\tTwice\(int i\);$', r'^12\|\t\}$', r'^13\|\}$',
		 r'^16\|#endif', r'^Disk lines Drawing\.cpp:$', r'^22\|int Geometry::Detail::Twice\(int i\)$'],
		[r'^\d+\|namespace Geometry\n\d+\|\{\n\d+\|\tnamespace Detail']),
	('move-function-namespace-variable', dict(MakeNamespacesCopy(), **{
		#giCount: its extern into Drawing.h's Geometry block, and its definition (a variable's is where it is declared
		#without extern) from the start of Shapes.cpp's Geometry block, with the blank lines after it, into the end of
		#Drawing.cpp's, a tab in.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '22:13',
		'DEBUG': 'edit:renamesearch:---s,edit:movablefunction,edit:movefunction:Drawing.h,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Drawing.h,printdisklines:Drawing.cpp'}),
		[r'^Move to File: Geometry::giCount, declared in Shapes\.h, defined in Shapes\.cpp, 2 files name it$', r'^Status: Moved Geometry::giCount to Drawing\.h and Drawing\.cpp: 5 files changed\.$',
		 r'^Disk lines Shapes\.h:$', r'^21\|\tTLength\tArea\(CPoint\* pcPoint\);$', r'^22\|$', r'^23\|\tnamespace Detail$',
		 r'^Disk lines Shapes\.cpp:$', r'^5\|namespace Geometry$', r'^6\|\{$', r'^7\|\t///',
		 r'^Disk lines Drawing\.h:$', r'^8\|$', r'^9\|\textern int\tgiCount;$', r'^10\|\}$',
		 r'^Disk lines Drawing\.cpp:$', r'^13\|\t\}$', r'^14\|$', r'^15\|$', r'^16\|\tint\tgiCount = 1;$', r'^17\|\}$'],
		[]),
	('member-from-function-namespace', dict(MakeNamespacesCopy(), **{
		#Convert Function to Member Function of a function in a namespace (Refactorings.txt 21 h): Geometry::Area is
		#CPoint's; its definition, written with its namespace, is Geometry::CPoint::Area; Main.cpp's call, found
		#through its using-directive, is cOther.Area(); its label is CPoint's.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:21:10,edit:functiontomember,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Status: Converted Geometry::Area to the member function Geometry::CPoint::Area: 1 call changed, 1 label in assembly\.$',
		 r'^18\|\t\tTLength\tArea\(void\);$', r'^19\|\t\};$', r'^22\|\textern int\tgiCount;$',
		 r'^35\|Geometry::TLength Geometry::CPoint::Area\(void\)$', r'^37\|\treturn Sum\(\) \* giCount;$',
		 r'^21\|\ti = cOther\.Area\(\) \+ Geometry::Detail::Twice\(giCount\);$', r'^7\|_Geometry__CPoint__Area:$'],
		[r'Area\(CPoint\*']),
	('member-from-function-other-namespace', dict(MakeNamespacesCopy(), **{
		#Drawing::Plot, whose parameter is a Geometry::CPoint*: Geometry::CPoint::Plot; the using-declaration of it in
		#Main.cpp goes, and the call through it is cPoint.Plot().
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:35:9,edit:functiontomember,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Status: Converted Drawing::Plot to the member function Geometry::CPoint::Plot: 1 call changed, 1 label in assembly\.$',
		 r'^18\|\t\tvoid\tPlot\(void\);$', r'^34\|\ttypedef long\tTLength;$', r'^35\|$', r'^36\|\textern "C" void\tClear\(void\);$',
		 r'^55\|void Geometry::CPoint::Plot\(void\)$', r'^57\|\tInit\(1, 2\);$',
		 r'^4\|using namespace Geometry;$', r'^5\|$', r'^19\|\tcPoint\.Plot\(\);$', r'^13\|_Geometry__CPoint__Plot:$'],
		[r'using Drawing::Plot', r'Plot\(Geometry::CPoint\*']),
	('template-parameter-namespace', dict(MakeNamespacesCopy(), **{
		#Extract Template Parameter of a function in a namespace (Refactorings.txt 21 h): Geometry::Area's declaration
		#in the Geometry block stays, template<class M> over it; its definition, written with its namespace, comes to
		#the header after the namespace's block (inside it, it couldn't be written with its namespace, and what it
		#uses may be declared after it).
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:21:15,edit:templateparameter,edit:extracttemplate:M,printdisklines:Shapes.h,printdisklines:Shapes.cpp'}),
		[r"^Extract Template Parameter: CPoint in Geometry::Area, the calls' arguments say it$",
		 r'^Status: Extracted the template parameter M from CPoint in Geometry::Area: its definition is in Shapes\.h now\.$',
		 r'^21\|\ttemplate<class M>$', r'^22\|\tTLength\tArea\(M\* pcPoint\);$', r'^23\|\textern int\tgiCount;$', r'^29\|\}$', r'^30\|$', r'^31\|$', r'^32\|///',
		 r'^36\|template<class M>$', r'^37\|Geometry::TLength Geometry::Area\(M\* pcPoint\)$', r'^39\|\treturn pcPoint->Sum\(\) \* giCount;$', r'^40\|\}$', r'^41\|$', r'^42\|$', r'^43\|namespace Drawing$'],
		[r'^\d+\|.*Area\(CPoint\*']),
	('template-parameter-qualified', dict(MakeNamespacesCopy(), **{
		#A type written with its namespace (Geometry::CPoint) is M as a whole; Main.cpp's using-declaration of Plot
		#names the function template as well, so it doesn't stop it.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:35:22,edit:templateparameter,edit:extracttemplate:M,printdisklines:Shapes.h,printdisklines:Shapes.cpp'}),
		[r"^Extract Template Parameter: CPoint in Drawing::Plot, the calls' arguments say it$",
		 r'^Status: Extracted the template parameter M from CPoint in Drawing::Plot: its definition is in Shapes\.h now\.$',
		 r'^35\|\ttemplate<class M>$', r'^36\|\tvoid\tPlot\(M\* pcPoint\);$', r'^39\|\}$', r'^40\|$', r'^41\|$', r'^\d+\|template<class M>$', r'^\d+\|void Drawing::Plot\(M\* pcPoint\)$'],
		[r'Geometry::M', r'^\d+\|void Drawing::Plot\(Geometry::CPoint']),
	('member-to-function-namespace', dict(MakeNamespacesCopy(), **{
		#Convert Member Function to Function of a class in a namespace: Geometry::CPoint::Sum is Geometry::Sum, declared
		#after the class at its indent in the Geometry block, defined where it was in Shapes.cpp's block; its label is
		#the function's.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:17:11,edit:membertofunction,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Status: Converted Geometry::CPoint::Sum to the function Sum\(CPoint\* pcPoint, \.\.\.\): 1 call changed, 2 labels in assembly\.$',
		 r'^17\|\t\};$', r'^18\|$', r'^19\|$', r'^20\|\tTLength\tSum\(CPoint\* pcPoint\);$',
		 r'^24\|\tTLength Sum\(CPoint\* pcPoint\)$', r'^26\|\t\treturn pcPoint->miX \+ pcPoint->miY;$', r'^37\|\treturn Sum\(pcPoint\) \* giCount;$',
		 r'^5\|_Geometry__Sum__FPC16Geometry__CPoint:$', r'^8\|\t\tjsr _Geometry__Sum__FPC16Geometry__CPoint$'],
		[r'^[0-9]+[|].*CPoint::Sum']),
	('move-function-variable-template', dict(MakeNamespacesCopy(), **{
		#Move to File of a variable template (Refactorings.txt 21): gmPi goes to Drawing.h whole, with its
		#template<class M>, and its explicit specialisation gmPi<int> after it (a declaration of it to the index, in a
		#template<> declaration); Use.cpp, which names it, #includes Drawing.h.
		'WAIT_INDEX': '1', 'OPEN': 'Constants.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:6:3,edit:movablefunction,edit:movefunction:Drawing.h,printdisklines:Constants.h,printdisklines:Drawing.h,printdisklines:Use.cpp'}),
		[r'^Move to File: gmPi, defined in Constants\.h, specialised in Constants\.h, 1 file names it$', r'^Status: Moved gmPi to Drawing\.h: 3 files changed\.$',
		 r'^Disk lines Constants\.h:$', r'^5\|namespace Geometry$', r'^12 lines$',
		 r'^Disk lines Drawing\.h:$', r'^8\|\}$', r'^9\|$', r'^10\|$', r'^11\|template<class M>$', r'^12\|M\tgmPi = M\(3\.14159265\);$', r'^13\|$', r'^14\|$', r'^15\|template<>$', r'^16\|int\tgmPi<int> = 3;$', r'^19\|#endif',
		 r'^Disk lines Use\.cpp:$', r'^1\|#include "Constants\.h"$', r'^2\|#include "Drawing\.h"$'],
		[]),
	('move-function-variable-template-namespace', dict(MakeNamespacesCopy(), **{
		#Geometry::gmTau into Drawing.h's Geometry block, with its template<class M>; gmPi's specialisation, which isn't
		#gmTau's, stays.
		'WAIT_INDEX': '1', 'OPEN': 'Constants.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:16:4,edit:movablefunction,edit:movefunction:Drawing.h,printdisklines:Constants.h,printdisklines:Drawing.h,printdisklines:Use.cpp'}),
		[r'^Move to File: Geometry::gmTau, defined in Constants\.h, 1 file names it$',
		 r'^Disk lines Constants\.h:$', r'^9\|template<>$', r'^10\|int\tgmPi<int> = 3;$', r'^13\|#endif',
		 r'^Disk lines Drawing\.h:$', r'^7\|\tint\tHalf\(int i\);$', r'^8\|$', r'^9\|\ttemplate<class M>$', r'^10\|\tM\tgmTau = M\(6\.2831853\);$', r'^11\|\}$'],
		[r'^\d+\|namespace Geometry\n(?s:.*)namespace Geometry\n(?s:.*)Disk lines Use']),
	('move-namespace-namespace', dict(MakeNamespacesCopy(), **{
		#Move to Namespace of a namespace (Refactorings.txt 21 a): Geometry::Detail to the global namespace.  Its block,
		#alone at the end of Geometry's, follows it; Geometry::Detail::Twice is Detail::Twice where it was written so,
		#and Detail::Twice in Geometry is still that; its labels lose Geometry.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:24:13,edit:movetonamespace:,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace: Geometry::Detail from \[Geometry\], 1 declarations: Shapes\.h\[1\]$',
		 r'^Move to Namespace status: Moved Geometry::Detail to the global namespace: 3 uses; 2 labels in assembly changed\.$',
		 r'^22\|\textern int\tgiCount;$', r'^23\|\}$', r'^26\|namespace Detail$', r'^27\|\{$', r'^28\|\tint\tTwice\(int i\);$', r'^29\|\}$', r'^32\|namespace Drawing$',
		 r'^16\|\t\tmiY = Detail::Twice\(iY\);$', r'^45\|int Detail::Twice\(int i\)$',
		 r'^21\|\ti = Area\(&cOther\) \+ Detail::Twice\(giCount\);$', r'^11\|_Detail__Twice__Fi:$', r'^14\|\t\tjsr _Detail__Twice__Fi$'],
		[r'Geometry::Detail::Twice\(', r'Geometry__Detail']),
	('move-namespace-namespace-into', dict(MakeNamespacesCopy(), **{
		#Geometry::Detail into Drawing: into the Drawing block next to it, at its start; every use Drawing::Detail.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:24:13,edit:movetonamespace:Drawing,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace status: Moved Geometry::Detail to Drawing: 3 uses; 2 labels in assembly changed\.$',
		 r'^23\|\}$', r'^26\|namespace Drawing$', r'^27\|\{$', r'^28\|\tnamespace Detail$', r'^29\|\t\{$', r'^30\|\t\tint\tTwice\(int i\);$', r'^31\|\t\}$', r'^32\|$', r'^33\|\ttypedef long\tTLength;$',
		 r'^16\|\t\tmiY = Drawing::Detail::Twice\(iY\);$', r'^45\|int Drawing::Detail::Twice\(int i\)$',
		 r'^21\|\ti = Area\(&cOther\) \+ Drawing::Detail::Twice\(giCount\);$', r'^11\|_Drawing__Detail__Twice__Fi:$'],
		[]),
	('move-namespace-namespace-top', dict(MakeNamespacesCopy(), **{
		#Drawing, a namespace of the file's, into Geometry: into the end of the Geometry block before it, a blank line
		#from Detail, and its qualified definition and using-declaration name Geometry::Drawing.  Geometry can't go
		#into its own Detail.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:31:11,edit:movetonamespace:Geometry,edit:caret:5:11,edit:movetonamespace:Geometry::Detail,printdisklines:Shapes.h,printdisklines:Shapes.cpp,printdisklines:Main.cpp,printdisklines:ShapesW65C816.asm'}),
		[r'^Move to Namespace status: Moved Drawing to Geometry: 2 uses; 1 label in assembly changed\.$',
		 r"^Move to Namespace status: Move to Namespace: a namespace can't go into itself\.$",
		 r'^27\|\t\}$', r'^28\|$', r'^29\|\tnamespace Drawing$', r'^30\|\t\{$', r'^31\|\t\ttypedef long\tTLength;$', r'^36\|\t\}$', r'^37\|\}$', r'^38\|$', r'^39\|$', r'^40\|#endif',
		 r'^55\|void Geometry::Drawing::Plot\(Geometry::CPoint\* pcPoint\)$', r'^5\|using Geometry::Drawing::Plot;$', r'^13\|_Geometry__Drawing__Plot__FPC16Geometry__CPoint:$'],
		[]),
	('move-namespace-refused', dict(MakeNamespacesCopy(), **{
		#A member of a class isn't at namespace scope: there is nothing to move.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:16:9,edit:movablenamespace'}),
		[r'^Move to Namespace: nothing$'], [r'declarations']),
	('move-class', dict(MakeMoveMembersCopy(), **{
		#Move Class to Its Own Files: CKeg shares Drum.h with CDrum and Drum.cpp with its functions.  It gets Keg.h (the
		#C++ Header template's: the Include Guard template's guard, Drum.h's #includes, the class) and Keg.cpp (the C++
		#Source template's: an #include of Keg.h, Drum.cpp's includes, its functions with the comments over them); the
		#project's licence is None, so neither has a File Header.  They go out of Drum.h and Drum.cpp, which don't end with
		#blank lines for it; Main.cpp, which names CKeg, #includes Keg.h beside its #include of Drum.h; and the project
		#has the new files beside the old.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '27:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movableclass,edit:moveclass:,printdisklines:Keg.h,printdisklines:Keg.cpp,printdisklines:Drum.h,printdisklines:Drum.cpp,'
		         'printdisklines:Main.cpp,printdisklines:MoveMembers.vcxproj'}),
		[r'^Move Class: CKeg from Drum\.h, 2 outside \(Drum\.cpp\), to Keg$', r'^Status: Moved CKeg to Keg\.h and Keg\.cpp: 2 definitions outside it, 1 #include added\.$', r'^Move Class: moved$',
		 r'^1\|#ifndef __KEG_H__$', r'^2\|#define __KEG_H__$', r'^3\|#include "Vessel\.h"$', r'^4\|$', r'^5\|$', r'^6\|class CKeg : public CVessel$', r'^10\|\tvoid\tVent\(void\) override;$', r'^11\|\};$',
		 r'^12\|$', r'^13\|$', r'^14\|#endif // __KEG_H__$', r'^14 lines$',
		 r'^1\|#include "Keg\.h"$', r'^2\|#include "Drum\.h"$', r'^3\|$', r'^4\|$', r'^5\|/{74}$', r'^9\|void CKeg::Tap\(void\)$', r'^12\|\}$', r'^13\|$', r'^14\|$', r'^15\|/{74}$',
		 r'^19\|void CKeg::Vent\(void\)$', r'^22\|\}$', r'^22 lines$',
		 r'^23\|\tvoid\tHide\(void\);$', r'^24\|\};$', r'^25\|$', r'^26\|$', r'^27\|#endif // __DRUM_H__$', r'^27 lines$',
		 r'^59\|void CDrum::Hide\(void\)$', r'^62\|\}$', r'^62 lines$',
		 r'^1\|#include "Drum\.h"$', r'^2\|#include "Keg\.h"$', r'^3\|#include "Crate\.h"$',
		 r'^\d+\|    <ClCompile Include="Drum\.cpp" />$', r'^\d+\|    <ClCompile Include="Keg\.cpp" />$', r'^\d+\|    <ClInclude Include="Drum\.h" />$', r'^\d+\|    <ClInclude Include="Keg\.h" />$'],
		[]),
	('new-class', dict(MakeMoveMembersCopy(), **{
		#File, New, C++ Class with Drum.h open: CBarrel's Barrel.h and Barrel.cpp, from the C++ Class template and its
		#C++ Class Source child, made beside Drum.h in its project (whose licence is None: no File Header), added to the
		#project, and opened.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'DIALOG_KEYS': 'type:CBarrel', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/New/C++ Class...,answered:1,printstatus,printdisklines:Barrel.h,printdisklines:Barrel.cpp,printdiskfile:MoveMembers.vcxproj,printtabs'}),
		[r'^Dialog \[New C\+\+ Class\] answered OK$', r'^Status: Made Barrel\.h and Barrel\.cpp from the C\+\+ Class template\.$',
		 r'^1\|#ifndef __BARREL_H__$', r'^2\|#define __BARREL_H__$', r'^3\|$', r'^4\|$', r'^5\|class CBarrel$', r'^6\|\{$', r'^7\|public:$', r'^8\|\tvoid\tInit\(void\);$',
		 r'^10\|\};$', r'^11\|$', r'^12\|$', r'^13\|#endif // __BARREL_H__$', r'^13 lines$',
		 r'^1\|#include "Barrel\.h"$', r'^2\|$', r'^3\|$', r'^4\|/{74}$', r'^8\|void CBarrel::Init\(void\)$', r'^9\|\{$', r'^10\|\}$', r'^17\|void CBarrel::Kill\(void\)$', r'^19\|\}$', r'^19 lines$',
		 r'<ClCompile Include="Barrel\.cpp" />', r'<ClInclude Include="Barrel\.h" />', r'^Tabs: .*Barrel\.h Barrel\.cpp$'],
		[r'COPYRIGHT', r'^Status: New:']),
	('new-class-undo', dict(MakeMoveMembersCopy(), **{
		#One undo takes New's files away, off the disk and out of the project; redo makes them again, in the project
		#(where New put them: nothing beside them was copied), with what they had.  The first press of each shows it.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'DIALOG_KEYS': 'type:CBarrel', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/New/C++ Class...,answered:1,printprojectfiles,edit:undoall,edit:undoall,edit:saveall,printprojectfiles,printdisklines:Barrel.h,'
				 'edit:redoall,edit:redoall,printprojectfiles,printdisklines:Barrel.h'}),
		[r'^Project files: Main\.cpp Rack\.cpp Vessel\.cpp Drum\.cpp Pair\.cpp Barrel\.cpp Vessel\.h Drum\.h Crate\.h Pair\.h Rack\.h Barrel\.h$',
		 r'^Project files: Main\.cpp Rack\.cpp Vessel\.cpp Drum\.cpp Pair\.cpp Vessel\.h Drum\.h Crate\.h Pair\.h Rack\.h$', r'^Disk lines Barrel\.h:$', r'^\(none\)$',
		 r'^Project files: Main\.cpp Rack\.cpp Vessel\.cpp Drum\.cpp Pair\.cpp Barrel\.cpp Vessel\.h Drum\.h Crate\.h Pair\.h Rack\.h Barrel\.h$',
		 r'^5\|class CBarrel$', r'^13 lines$'],
		[]),
	('new-class-refused', dict(MakeMoveMembersCopy(), **{
		#New with a name a file of the solution has (CDrum's Drum.h): refused, and nothing is made.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'DIALOG_KEYS': 'type:CDrum', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/New/C++ Class...,answered:1,printstatus'}),
		[r'^Status: New: a file of the solution is called Drum\.h already\.$'],
		[r'^Status: Made']),
	('new-asks-variables', dict(MakeMoveMembersCopy(None, [('Files', 'C++ Header', '// ${DESCRIPTION}\n#parse("Include Guard Open")\n#parse("Include Guard Close")\n', 'h', '${NAME}')]), **{
		#A Workspace scheme's C++ Header that uses ${DESCRIPTION}, which nothing gives: New's dialog has a field for
		#it after the name, and the file has what is written in it.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'DIALOG_KEYS': 'type:Pump,tab,type:A pump', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/New/C++ Header...,answered:1,printstatus,printdisklines:Pump.h'}),
		[r'^Status: Made Pump\.h from the C\+\+ Header template\.$', r'^1\|// A pump$', r'^2\|#ifndef __PUMP_H__$', r'^4\|#endif // __PUMP_H__$', r'^4 lines$'],
		[r'^Status: New:']),
	('save-file-as-template', dict(MakeMoveMembersCopy(), **{
		#File, Save File as Template with Drum.h open: a Workspace scheme's Files template called Drum, its guard and
		#its class made their variables and its files named by the class's stem; New from it makes CTun's Tun.h.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'DIALOG_KEYS': '|type:CTun', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'command:File.SaveAsTemplate,answered:1,printstatus,printdisklines:fileTemplates/Files/Drum.h.ft,clickmenu:File/New/Drum...,answered:2,printstatus,printdisklines:Tun.h'}),
		[r"^Status: Saved Drum\.h as the Workspace scheme's template Drum: finish it in Settings, File and Code Templates\.$",
		 r'^1\|#ifndef \$\{INCLUDE_GUARD\}$', r'^2\|#define \$\{INCLUDE_GUARD\}$', r'^6\|class \$\{CLASS_NAME\} : public CVessel$', r'^27\|class CKeg : public CVessel$',
		 r'^Status: Made Tun\.h from the Drum template\.$', r'^1\|#ifndef __TUN_H__$', r'^6\|class CTun : public CVessel$'],
		[r'^Status: New:', r'Save File as Template:']),
	('new-header', dict(MakeMoveMembersCopy(), **{
		#New C++ Header: one file named as written, from the C++ Header template.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'DIALOG_KEYS': 'type:Pump', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/New/C++ Header...,answered:1,printstatus,printdisklines:Pump.h'}),
		[r'^Status: Made Pump\.h from the C\+\+ Header template\.$', r'^1\|#ifndef __PUMP_H__$', r'^2\|#define __PUMP_H__$', r'^\d+\|#endif // __PUMP_H__$'],
		[r'^Status: New:']),
	('project-licence', dict(MakeMoveMembersCopy(), **{
		#Properties... on a project in Solution Explorer: its licence (None, as its .hcp65project has it) made LGPL 3,
		#written to the .hcp65project.
		'WAIT_INDEX': '1', 'DIALOG_KEYS': 'up,up', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:explorerselect:MoveMembers,command:Explorer.ProjectProperties,answered:1,printdisklines:MoveMembers.hcp65project'}),
		[r'^Dialog \[MoveMembers Properties\] answered OK$', r"^Status: MoveMembers's new files are written with the licence LGPL 3\.$",
		 r'^\d+\|<HCP65Project Licence="LGPL 3"/>$'],
		[r'Licence="None"']),
	('move-class-licence', dict(MakeMoveMembersCopy('LGPL 3'), **{
		#Move Class with the project's licence LGPL 3 (its .hcp65project): Keg.h and Keg.cpp start with the File Header
		#template's licence comment, naming the project, the year and the author, and then are as move-class's.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '27:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movableclass,edit:moveclass:,printdisklines:Keg.h,printdisklines:Keg.cpp'}),
		[r'^Status: Moved CKeg to Keg\.h and Keg\.cpp: 2 definitions outside it, 1 #include added\.$',
		 r'^1\|/\*\* -+ COPYRIGHT NOTICE, DISCLAIMER, and LICENSE -+ \*\*$', r'^3\|Copyright \(c\) \d{4} \S.*$', r'^5\|This file is part of The Codaphela Project: Codaphela MoveMembers$',
		 r'^8\|it under the terms of the GNU Lesser General Public License as published by$', r'^20\|\*\* -+ \*\*/$', r'^21\|#ifndef __KEG_H__$', r'^22\|#define __KEG_H__$',
		 r'^23\|#include "Vessel\.h"$', r'^26\|class CKeg : public CVessel$', r'^34\|#endif // __KEG_H__$', r'^34 lines$',
		 r'^20\|\*\* -+ \*\*/$', r'^21\|#include "Keg\.h"$', r'^22\|#include "Drum\.h"$', r'^29\|void CKeg::Tap\(void\)$', r'^42 lines$'], []),
	('move-class-template', dict(MakeMoveMembersCopy(), **{
		#A class template: its member defined outside it in the header goes to the new header with it, there is no
		#source file, and the new header #includes the old one, where the class it derives from stays.  A name that is
		#taken, or isn't a file's, is refused.
		'WAIT_INDEX': '1', 'OPEN': 'Crate.h', 'CARET': '16:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movableclass,edit:moveclass:Crate,edit:moveclass:Big Crate,edit:moveclass:BigCrate,printdisklines:BigCrate.h,printdisklines:Crate.h,printdisklines:Main.cpp'}),
		[r'^Move Class: CCrate from Crate\.h, 1 outside, to Crate$', r'^Move Class: Crate\.h is there already$', r"^Move Class: the files' name is letters, digits, _ and -, without an extension$",
		 r'^Status: Moved CCrate to BigCrate\.h: 1 definition outside it, 1 #include added\.$',
		 r'^1\|#ifndef __BIG_CRATE_H__$', r'^2\|#define __BIG_CRATE_H__$', r'^3\|#include "Crate\.h"$', r'^4\|$', r'^5\|$', r'^6\|template<class M>$', r'^7\|class CCrate : public CBox<M>$', r'^11\|\};$',
		 r'^12\|$', r'^13\|$', r'^14\|template<class M>$', r'^15\|int CCrate<M>::Size\(void\)$', r'^18\|\}$', r'^21\|#endif // __BIG_CRATE_H__$',
		 r'^12\|\};$', r'^13\|$', r'^14\|$', r'^15\|#endif // __CRATE_H__$', r'^15 lines$',
		 r'^2\|#include "Crate\.h"$', r'^3\|#include "BigCrate\.h"$'],
		[]),
	('move-class-refused', dict(MakeMoveMembersCopy(), **{
		#A class that is all its header declares is renamed, not moved; one that needs what stays in its header, which
		#needs it, can't have a header of its own; and no class at the caret.
		'WAIT_INDEX': '1', 'OPEN': 'Vessel.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movableclass,edit:caret:3:1,edit:movableclass,edit:open:Pair.h,edit:caret:17:8,edit:movableclass,edit:moveclass:'}),
		[r"^Move Class: CVessel can't be moved: it is the only thing Vessel\.h declares: Rename File renames its files$", r'^Move Class: no class$',
		 r"^Move Class: CRight can't be moved: it and what stays in Pair\.h each name the other: their headers would each have to #include the other$"],
		[r'^Move Class: moved', r'^Status: Moved']),
	('move-class-undo', dict(MakeMoveMembersCopy(), **{
		#One undo puts the class back and takes the new files away, off the disk and out of the project: what builds
		#is what built before.  Redo makes them again, with what they had, and moves the class again.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '27:8',
		'DEBUG': 'edit:renamesearch:---s,edit:moveclass:,printprojectfiles,edit:undoall,edit:undoall,edit:saveall,printprojectfiles,printdisklines:Keg.h,printdisklines:Keg.cpp,printdisklines:Drum.h,printdisklines:Drum.cpp,printdisklines:Main.cpp,'
				 'edit:redoall,edit:redoall,printprojectfiles,printdisklines:Keg.h,printdisklines:Keg.cpp,printdisklines:Drum.h,printtabs'}),
		[r'^Status: Moved CKeg to Keg\.h and Keg\.cpp', r'^Project files: Main\.cpp Rack\.cpp Vessel\.cpp Drum\.cpp Keg\.cpp Pair\.cpp Vessel\.h Drum\.h Keg\.h Crate\.h Pair\.h Rack\.h$',
		 r'^Project files: Main\.cpp Rack\.cpp Vessel\.cpp Drum\.cpp Pair\.cpp Vessel\.h Drum\.h Crate\.h Pair\.h Rack\.h$',
		 r'^Disk lines Keg\.h:$', r'^\(none\)$', r'^Disk lines Keg\.cpp:$', r'^\(none\)$',
		 r'^27\|class CKeg : public CVessel$', r'^35 lines$', r'^79\|void CKeg::Vent\(void\)$', r'^82 lines$', r'^1\|#include "Drum\.h"$', r'^2\|#include "Crate\.h"$',
		 r'^Project files: Main\.cpp Rack\.cpp Vessel\.cpp Drum\.cpp Keg\.cpp Pair\.cpp Vessel\.h Drum\.h Keg\.h Crate\.h Pair\.h Rack\.h$',
		 r'^Disk lines Keg\.h:$', r'^6\|class CKeg : public CVessel$', r'^14 lines$', r'^Disk lines Keg\.cpp:$', r'^1\|#include "Keg\.h"$', r'^22 lines$', r'^27 lines$'],
		[]),
	('move-class-dialog', dict(MakeMoveMembersCopy(), **{
		#The dialog: the files' name, suggested from the class's.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '27:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'moveclass,answered:1,printstatus'}),
		[r'^Dialog \[Move CKeg to Its Own Files\] answered OK$', r'^Status: Moved CKeg to Keg\.h and Keg\.cpp: 2 definitions outside it, 1 #include added\.$'],
		[]),
	('move-function-completion', dict(MakeDefinitionsCopy(), **{
		#Move to File's dialog: the field completes over the solution's headers as it is typed in, down and enter take
		#one; the button beside it opens the chooser of the solution's files on that header, where two downs choose
		#the next header, which goes in the field; and the function moves there.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '5:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:Ta,down,enter,tab,space|down,down', 'DIALOG_ANSWER': 'OK|OK',
		'DEBUG': 'edit:renamesearch:---s,moveclass,answered:2,printstatus,edit:saveall,printdisklines:Valve.h'}),
		[r'^Focus: CCompletionField$', r'^Field \[Ta\] completes: Tank\.h$', r'^Field \[Ta\] completes: \*Tank\.h$', r'^Field \[Tank\.h\]$', r'^Focus: CButton$',
		 r'^Tree \[Valve\.cpp\]$', r'^Tree \[Valve\.h\]$', r'^Dialog \[Choose the Header\] answered OK$', r'^Field \[Valve\.h\]$',
		 r'^Dialog \[Move ReadGauge to File\] answered OK$', r'^Status line: Moved ReadGauge to Valve\.h and Valve\.cpp: 5 files changed\.$',
		 r'^\d+\|int\t+ReadGauge\(int iGauge, int iScale = 1\);$'],
		[]),
	('move-function', dict(MakeDefinitionsCopy(), **{
		#Move to File (F6 on a function that is in no class): ReadGauge's prototype goes to Tank.h, after the function
		#Tank.h declares, with its default value; its definition goes to the end of Tank.cpp with the comment over it;
		#Tank.cpp #includes Gauge.h, which declares the ClampGauge its body calls; they go out of Gauge.h and
		#Gauge.cpp.  Main.cpp, which calls it, #includes Tank.h already.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movablefunction,edit:movefunction:Tank.h,printdisklines:Gauge.h,printdisklines:Gauge.cpp,printdisklines:Tank.h,printdisklines:Tank.cpp,printdisklines:Main.cpp'}),
		[r'^Move to File: ReadGauge\(int, int\), declared in Gauge\.h, defined in Gauge\.cpp, 1 file names it$', r'^Status: Moved ReadGauge to Tank\.h and Tank\.cpp: 4 files changed\.$', r'^Move to File: moved$',
		 r'^5\|int\t\tResetGauge\(int iGauge\);$', r'^6\|$', r'^29 lines$',
		 r'^1\|#include "Gauge\.h"$', r'^8\|int ResetGauge\(int iGauge\)$', r'^11 lines$',
		 r'^49\|int\t\tMeasureTank\(CTank\* pcTank\);$', r'^50\|int\t\tReadGauge\(int iGauge, int iScale = 1\);$', r'^51\|$', r'^52\|$', r'^53\|#endif // __TANK_H__$',
		 r'^1\|#include "Tank\.h"$', r'^2\|#include "Gauge\.h"$', r'^3\|$', r'^66\|\}$', r'^67\|$', r'^68\|$', r'^69\|/{74}$', r'^73\|int ReadGauge\(int iIndex, int iScale\)$',
		 r'^75\|\treturn ClampGauge\(iIndex \* iScale, 100\);$', r'^76 lines$',
		 r'^1\|#include "Tank\.h"$', r'^2\|#include "Gauge\.h"$', r'^3\|$', r'^18 lines$'],
		[]),
	('move-function-inline', dict(MakeDefinitionsCopy(), **{
		#A function a header defines goes to the header it is sent to, before its #endif; the file that calls it
		#(Gauge.cpp) #includes that header beside... Gauge.cpp has no #include of a header it was declared in but its
		#own, so after it.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '13:14',
		'DEBUG': 'edit:renamesearch:---s,edit:movablefunction,edit:movefunction:Valve.h,printdisklines:Gauge.h,printdisklines:Gauge.cpp,printdisklines:Valve.h'}),
		[r'^Move to File: ClampGauge\(int, int\), defined in Gauge\.h, 1 file names it$', r'^Status: Moved ClampGauge to Valve\.h: 3 files changed\.$',
		 r'^6\|int\t\tResetGauge\(int iGauge\);$', r'^7\|$', r'^8\|$', r'^9\|/{74}$', r'^13\|template<class M>$', r'^20 lines$',
		 r'^1\|#include "Gauge\.h"$', r'^2\|#include "Valve\.h"$', r'^3\|$', r'^22 lines$',
		 r'^15\|\};$', r'^16\|$', r'^17\|$', r'^18\|/{74}$', r'^22\|inline int ClampGauge\(int iValue, int iMax = 100\)$', r'^25\|\}$', r'^26\|$', r'^27\|$', r'^28\|#endif // __VALVE_H__$'],
		[]),
	('move-function-refused', dict(MakeDefinitionsCopy(), **{
		#Where it can't go, and what isn't a function outside a class.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '6:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movefunction:Nope.h,edit:movefunction:Gauge.h,edit:movefunction:Tank.cpp,edit:caret:3:1,edit:movablefunction,edit:open:Tank.h,edit:caret:14:8,edit:movablefunction'}),
		[r'^Move to File: there is no Nope\.h in the solution$', r'^Move to File: it is in Gauge\.h already$', r'^Move to File: it goes to a header: a \.h file of the solution$',
		 r'^Move to File: no function$', r"^Move to File: CTank::Fill\(int, int\) can't be moved: it is in CTank$"],
		[r'^Move to File: moved']),
	('move-function-undo', dict(MakeDefinitionsCopy(), **{
		#One undo across the four files.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:---s,edit:movefunction:Tank.h,edit:undoall,edit:undoall,edit:saveall,printdisklines:Gauge.h,printdisklines:Gauge.cpp,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r'^Status: Moved ReadGauge to Tank\.h and Tank\.cpp', r'^5\|int\t\tReadGauge\(int iGauge, int iScale = 1\);$', r'^30 lines$', r'^8\|int ReadGauge\(int iIndex, int iScale\)$', r'^21 lines$',
		 r'^49\|int\t\tMeasureTank\(CTank\* pcTank\);$', r'^50\|$', r'^52 lines$', r'^1\|#include "Tank\.h"$', r'^2\|$', r'^65 lines$'],
		[]),
	('move-function-dialog', dict(MakeDefinitionsCopy(), **{
		#The dialog asks for the header; the same F6 that moves a class.
		'WAIT_INDEX': '1', 'OPEN': 'Gauge.h', 'CARET': '5:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'moveclass,answered:1,printstatus'}),
		[r'^Dialog \[Move ReadGauge to File\] answered Cancel$'],
		[r'Moved ReadGauge']),
	('copy-class', dict(MakeDefinitionsCopy(), **{
		#Copy Class: CTank as CBarrel, in Barrel.h and Barrel.cpp made as Move Class makes them, with the class's name
		#changed where the copied text names it (the class, its constructor in the class and outside it, the name
		#before each member's definition), and Tank.h and Tank.cpp as they were.  A name that is the class's own, or
		#taken, or no name, is refused.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:---s,edit:copyclass:CTank,edit:copyclass:CStore,edit:copyclass:9x,edit:copyclass:CBarrel,printdisklines:Barrel.h,printdisklines:Barrel.cpp,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r'^Copy Class: the copy has another name$', r'^Copy Class: there is a CStore already$', r"^Copy Class: the copy's name is letters, digits and _$",
		 r'^Status: Copied CTank as CBarrel to Barrel\.h and Barrel\.cpp: 4 definitions outside it\.$', r'^Copy Class: copied$',
		 r'^1\|#ifndef __BARREL_H__$', r'^5\|class CBarrel$', r'^12\|\t\t\tCBarrel\(int iSize\);$', r'^13\|\tvoid\tInit\(int iSize\);$', r'^27\|\};$', r'^30\|#endif // __BARREL_H__$', r'^30 lines$',
		 r'^1\|#include "Barrel\.h"$', r'^2\|#include "Tank\.h"$', r'^9\|CBarrel::CBarrel\(int iCapacity\) : miLevel\(0\), miSize\(iCapacity\)$', r'^18\|void CBarrel::Init\(int iSize\)$',
		 r'^27\|// Whether it holds all it can\.$', r'^30\|bool CBarrel::IsFull\(void\) const$', r'^40\|void CBarrel::Empty\(void\)$', r'^46 lines$',
		 r'^5\|class CTank$', r'^12\|\t\t\tCTank\(int iSize\);$', r'^52 lines$', r'^8\|CTank::CTank\(int iCapacity\) : miLevel\(0\), miSize\(iCapacity\)$', r'^65 lines$'],
		[]),
	('copy-class-comments', dict(MakeDefinitionsCopy(), **{
		#Copy Class renames the class in the copy's comments too (question 27): a comment naming CTank names CBarrel in
		#Barrel.h, a word that only starts with it (CTankLike) is left, and Tank.h keeps its own comment.  Alt+Insert's
		#menu has Copy Class after the Files templates.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'CARET': '5:8',
		'DEBUG': 'edit:replace:13|void\tInit(int iSize);|void\tInit(int iSize);  //A new CTank is empty: see CTankLike.,edit:renamesearch:---s,edit:copyclass:CBarrel,'
				 'printdisklines:Barrel.h,edit:saveall,printdisklines:Tank.h,printmenus'}),
		[r'^Status: Copied CTank as CBarrel to Barrel\.h and Barrel\.cpp', r'^5\|class CBarrel$', r'^13\|\tvoid\tInit\(int iSize\);  //A new CBarrel is empty: see CTankLike\.$',
		 r'^13\|\tvoid\tInit\(int iSize\);  //A new CTank is empty: see CTankLike\.$', r'^Menu New: C\+\+ Class\.\.\., C\+\+ Header\.\.\., C\+\+ Source\.\.\., Copy Class\.\.\.'],
		[r'CBarrelLike']),
	('copy-class-template', dict(MakeMoveMembersCopy(), **{
		#A class template, with the member its header defines outside it; the copy's header #includes the one it was
		#copied from, where the class it derives from is.
		'WAIT_INDEX': '1', 'OPEN': 'Crate.h', 'CARET': '16:8',
		'DEBUG': 'edit:renamesearch:---s,edit:copyclass:CChest,printdisklines:Chest.h,printdisklines:Crate.h'}),
		[r'^Status: Copied CCrate as CChest to Chest\.h: 1 definition outside it\.$',
		 r'^1\|#ifndef __CHEST_H__$', r'^3\|#include "Crate\.h"$', r'^6\|template<class M>$', r'^7\|class CChest : public CBox<M>$', r'^14\|template<class M>$', r'^15\|int CChest<M>::Size\(void\)$', r'^21 lines$',
		 r'^16\|class CCrate : public CBox<M>$', r'^24\|int CCrate<M>::Size\(void\)$', r'^30 lines$'],
		[]),
	('copy-class-dialog', dict(MakeMoveMembersCopy(), **{
		#The dialog starts with the class's own name, which the copy can't have: OK is refused, and it says why.
		'WAIT_INDEX': '1', 'OPEN': 'Vessel.h', 'CARET': '5:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'copyclass,answered:1,printstatus'}),
		[r'^Dialog \[Copy CVessel\] answered Cancel$'],
		[r'Copied']),
	('inline-variable', dict(MakeInlineVariableCopy(), **{
		#Inline Variable: iSum is declared at the top of its function and given its value by one statement; both go,
		#and its value is written where it was used, in parentheses where the expression would take it apart
		#(iSum * 2).  Then iResult, from a use of it: its value is a call, used once.
		'WAIT_INDEX': '1', 'OPEN': 'Flow.cpp', 'CARET': '10:6',
		'DEBUG': 'edit:renamesearch:---s,edit:inlinable,edit:inlinevariable,edit:caret:13:10,edit:inlinevariable,printdisklines:Flow.cpp'}),
		[r'^Inline Variable: iSum = \[iA \+ iB\], 2 uses, 1 in parentheses, assigned by a statement$', r'^Status: Inlined iSum: 2 uses\.$',
		 r'^Inline Variable: iResult = \[\(iA \+ iB\) \* 2 \+ Scale\(iA \+ iB\)\], 1 uses, 0 in parentheses, assigned by a statement$', r'^Status: Inlined iResult: 1 use\.$',
		 r'^8\|int FlowSum\(int iA, int iB\)$', r'^9\|\{$', r'^10\|\treturn \(iA \+ iB\) \* 2 \+ Scale\(iA \+ iB\);$', r'^11\|\}$', r'^12\|$', r'^13\|$', r'^14\|/{74}$'],
		[r'^Conflict']),
	('inline-constant', dict(MakeInlineVariableCopy(), **{
		#Inline for a constant of a file or a class: its value takes the place of every use, in whatever file, in
		#parentheses where it needs them, and its declaration goes; from a use, that use alone can be, the constant
		#staying.  One whose value names a variable that isn't const isn't a constant.
		'WAIT_INDEX': '1', 'OPEN': 'Limits.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:8:12,edit:inlinableconstant,edit:open:Limits.cpp,edit:caret:13:46,edit:inlineconstant:this,'
				 'edit:open:Limits.h,edit:caret:6:12,edit:inlineconstant,edit:caret:5:12,edit:inlineconstant,printdisklines:Limits.h,printdisklines:Limits.cpp'}),
		[r"^Inline Constant: giLive can't be: its value names giCount, which isn't a constant \(Limits\.h line 8\)$",
		 r'^Inline Constant: miDepth = \[4\], 2 uses in 2 files, 1 declaration, the caret on a use$', r'^Status: Inlined miDepth = 4: 1 use; the constant stays\.$',
		 r'^Inline Constant: giSpare = \[giMostUnits \* 2 \+ 1\], 1 use in 1 file, 1 declaration$', r'^Status: Inlined giSpare = giMostUnits \* 2 \+ 1: 1 use, and the constant is taken out\.$',
		 r'^Inline Constant: giMostUnits = \[8\], 3 uses in 1 file, 1 declaration$', r'^Status: Inlined giMostUnits = 8: 3 uses, and the constant is taken out\.$',
		 r'^5\|int\t\t\tgiCount = 0;$', r'^6\|const int\tgiLive = giCount \+ 1;$', r'^12\|\tstatic const int\tmiDepth = 4;$', r'^18 lines$',
		 r'^10\|\tint\taiUnits\[8\];$', r'^12\|\taiUnits\[0\] = 8 \* iScale;$', r'^13\|\treturn aiUnits\[0\] \+ \(8 \* 2 \+ 1\) \* 3 \+ 4 \+ giLive;$'],
		[]),
	('inline-variable-conflicts', dict(MakeInlineVariableCopy(), **{
		#What stands in the way: a value that is a call and would be made twice; a variable the value reads that is
		#changed before the use.  Nothing is inlined until it is asked for all the same.  A call used once is inlined
		#without a word.
		'WAIT_INDEX': '1', 'OPEN': 'Flow.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:25:6,edit:inlinevariable,edit:caret:39:6,edit:inlinevariable,edit:caret:26:6,edit:inlinevariable,edit:caret:25:6,edit:inlinevariable:anyway,printdisklines:Flow.cpp'}),
		[r'^Inline Variable: iScaled = \[Scale\(iA\)\], 2 uses, 0 in parentheses, given by its declaration$', r'^Conflict: Its value does something \(a call, an assignment\), which would be done 2 times\.$',
		 r'^Inline Variable: iFirst = \[iA \* 2\], 1 uses, 0 in parentheses, assigned by a statement$', r'^Conflict: iA, which its value reads, is changed at line 45, before iFirst is last used\.$',
		 r'^Inline Variable: iOnce = \[Scale\(iA \+ 1\)\], 1 uses, 0 in parentheses, given by its declaration$', r'^Status: Inlined iOnce: 1 use\.$', r'^Status: Inlined iScaled: 2 uses\.$',
		 r'^23\|int FlowCall\(int iA\)$', r'^24\|\{$', r'^25\|\treturn Scale\(iA\) \+ Scale\(iA\) \+ Scale\(iA \+ 1\);$', r'^26\|\}$'],
		[r'Inlined iFirst']),
	('inline-variable-refused', dict(MakeInlineVariableCopy(), **{
		#What can't be inlined, and why (the compiler says how each is changed).
		'WAIT_INDEX': '1', 'OPEN': 'Flow.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:38:6,edit:inlinevariable,edit:caret:57:7,edit:inlinevariable,edit:caret:58:7,edit:inlinevariable,edit:caret:59:7,edit:inlinevariable,'
		         'edit:caret:60:7,edit:inlinevariable,edit:caret:61:7,edit:inlinevariable,edit:caret:8:6,edit:inlinevariable,edit:caret:8:18,edit:inlinevariable,printdisklines:Flow.cpp'}),
		[r"^Inline Variable: iCount can't be inlined: it is changed \(assigned in an expression, \+\+, \+=\) at line 43$", r"^Inline Variable: iX can't be inlined: it is declared with others in one declaration$",
		 r"^Inline Variable: iTaken can't be inlined: its address is taken at line 67$", r"^Inline Variable: iTwice can't be inlined: it is assigned more than once: again at line 69$",
		 r"^Inline Variable: iLate can't be inlined: it is given its value in a block that this use is outside: the use at line 74$", r"^Inline Variable: iNever can't be inlined: it is never given a value$",
		 r'^Inline Variable: no variable$', r'^85 lines$'],
		[r'^Status: Inlined']),
	('inline-variable-dialog', dict(MakeInlineVariableCopy(), **{
		#Ctrl+Alt+N's command: Problems Detected if anything is in the way (Cancel inlines nothing), and the status
		#line says why a variable can't be inlined.
		'WAIT_INDEX': '1', 'OPEN': 'Flow.cpp', 'CARET': '25:6', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'inlinevariable,answered:1,printstatus,edit:caret:38:6,inlinevariable,printstatus,edit:caret:26:6,inlinevariable,printstatus,edit:undo,printdisklines:Flow.cpp'}),
		[r'^Dialog \[Problems Detected\] answered Cancel$', r"^Status: Can't inline iCount: it is changed \(assigned in an expression, \+\+, \+=\) at line 43\.$", r'^Status: Inlined iOnce: 1 use\.$'],
		[r'Inlined iScaled']),
	('introduce-kinds', dict(MakeIntroduceVariableCopy(), **{
		#What the selection can be introduced as, and the name suggested for each: a field's with m before the
		#variable's, a constant's in capitals.  A call with the function's parameter can't be a constant, and a
		#member (Scaled is one) can't be passed by a call; a number can be anything.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:selecttext:66:12-66:26,edit:introduciblekinds,edit:selecttext:66:29-66:40,edit:introduciblekinds,edit:selecttext:67:19-67:20,edit:introduciblekinds'}),
		[r'^Introduce Field: miScaled$', r"^Introduce Constant: SCALED, can't be: the expression isn't a constant: it names a variable or does something$",
		 r"^Introduce Parameter: iScaled, can't be: the expression uses its class's members, which a call of the function names another way$",
		 r'^Introduce Field: miScale$', r"^Introduce Parameter: iScale, can't be: the expression uses its class's members, which a call of the function names another way$",
		 r'^Introduce Field: miValue$', r'^Introduce Constant: VALUE$', r'^Introduce Parameter: iValue$'],
		[]),
	('introduce-field', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Field: the expression becomes a field of the function's class, declared after the class's last
		#field and lined up with it, in the header; it is assigned before the first statement that uses it, and both
		#occurrences are replaced.  A name the class has is refused.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:66:29-66:40,edit:introducefield:,edit:introducefield:muiCount,edit:introducefield:miDouble,printdisklines:Calc.h,printdisklines:Calc.cpp'}),
		[r'^Status: Introduce Variable: The function uses that name already\.$', r'^Status: Introduce Variable: The class has that name already\.$',
		 r'^Status: Introduced the field int miDouble = miScale \* 2: 2 occurrences replaced\.$',
		 r'^13\|\tchar\*\tmszName;$', r'^14\|\tint\t\tmiDouble;$', r'^15\|$', r'^16\|\tvoid\t\t\t\tInit\(void\);$',
		 r'^62\|\tint\t\tiResult;$', r'^63\|\tCCalc\*\tpcOther;$', r'^64\|$', r'^65\|\tpcOther = this;$', r'^66\|\tmiDouble = miScale \* 2;$', r'^67\|\tiResult = Scaled\(iExtra\) \+ miDouble;$',
		 r'^70\|\t\tiResult \+= miDouble;$'],
		[]),
	('introduce-field-init', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Field with "Assign it in Init" (question 31): the field is declared as introduce-field's is, its
		#assignment goes at the end of CCalc::Init, and the occurrences are replaced with no assignment before them.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:66:29-66:40,edit:introducefieldinit:miDouble,printdisklines:Calc.h,printdisklines:Calc.cpp'}),
		[r'^Status: Introduced the field int miDouble = miScale \* 2: 2 occurrences replaced\.$', r'^14\|\tint\t\tmiDouble;$',
		 r'^8\|void CCalc::Init\(void\)$', r'^12\|\tmszName = 0;$', r'^13\|\tmiDouble = miScale \* 2;$', r'^14\|\}$',
		 r'^66\|\tpcOther = this;$', r'^67\|\tiResult = Scaled\(iExtra\) \+ miDouble;$', r'^70\|\t\tiResult \+= miDouble;$'],
		[r'^\d+\|\tmiDouble = miScale \* 2;\n.*iResult = Scaled']),
	('introduce-field-init-dialog', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Field's dialog offers "Assign it in Init" for an expression that names nothing of the function's own,
		#in a class that has an Init: ticked, the field is assigned there.  The keys: the name, then the two ticks
		#(Replace all, then Assign it in Init).
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp', 'DIALOG_KEYS': 'type:miDouble,tab,tab,space', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:66:29-66:40,command:Refactor.IntroduceField,answered:1,printstatus,edit:saveall,printdisklines:Calc.cpp'}),
		[r'^Dialog \[Introduce Field\] answered OK$', r'^Status: Introduced the field int miDouble = miScale \* 2: 2 occurrences replaced\.$',
		 r'^13\|\tmiDouble = miScale \* 2;$', r'^14\|\}$', r'^67\|\tiResult = Scaled\(iExtra\) \+ miDouble;$'],
		[]),
	('introduce-constant', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Constant: a number becomes a #define after the file's #include, two blank lines from it, and each
		#place it is written in the function its name; a second goes under the first.  An expression of more than one
		#thing is in parentheses.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:66:39-66:40,edit:introduceconstant:DOUBLE,edit:selecttext:70:19-70:20,edit:introduceconstant:,'
		         'edit:selecttext:109:29-109:38,edit:introduceconstant:CHOICE,printdisklines:Calc.cpp'}),
		[r'^Status: Introduced the constant DOUBLE = 2: 2 occurrences replaced\.$', r'^Status: Introduced the constant VALUE = 4: 1 occurrence replaced\.$',
		 r'^1\|#include "Calc\.h"$', r'^2\|$', r'^3\|$', r'^4\|#define DOUBLE\t2$', r'^5\|#define VALUE\t4$', r'^6\|',
		 r'\tiResult = Scaled\(iExtra\) \+ miScale \* DOUBLE;$', r'\tif \(GetCount\(\) > VALUE\)$', r'\t\tiResult \+= miScale \* DOUBLE;$'],
		[]),
	('introduce-constant-file', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Constant for every occurrence in the file, when that is asked for: the number is in more functions
		#than the one it is selected in.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:122:19-122:20,edit:introduceconstantfile:TWICE,printstatus,printdisklines:Calc.cpp'}),
		[r'^Introduce Constant: 2 in the function, 4 in the file$', r'^Status: Introduced the constant TWICE = 2: 4 occurrences replaced\.$',
		 r'^4\|#define TWICE\t2$', r'^69\|\tiResult = Scaled\(iExtra\) \+ miScale \* TWICE;$', r'^72\|\t\tiResult \+= miScale \* TWICE;$',
		 r'^125\|\tiTotal = iBase \* TWICE;$', r'^127\|\tiTotal \+= iBase \* TWICE;$'],
		[]),
	('introduce-parameter', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Parameter: the number becomes a parameter of its function, added last where the class declares it
		#and where it is defined (Change Signature), every call passes the number, and in the function it is the
		#parameter's name; all one undo.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:67:19-67:20,edit:introduceparameter:iLimit,printdisklines:Calc.h,printdisklines:Calc.cpp,printdisklines:Main.cpp,'
		         'edit:undoall,edit:undoall,edit:saveall,printdisklines:Calc.h'}),
		[r'^Status: Introduced the parameter int iLimit = 4: 1 occurrence replaced\.$',
		 r'^20\|\tint\t\t\t\t\tTotal\(int iExtra, int iLimit\);$', r'^60\|int CCalc::Total\(int iExtra, int iLimit\)$', r'^67\|\tif \(GetCount\(\) > iLimit\)$',
		 r'^12\|\treturn cCalc\.Total\(2, 4\) \+ cStack\.Depth\(\);$', r'^20\|\tint\t\t\t\t\tTotal\(int iExtra\);$'],
		[r'^Conflict']),
	('introduce-parameter-replace', dict(MakeIntroduceVariableCopy(), **{
		#Introduce Parameter of an expression of the function's parameters (question 32): each call passes the
		#expression with its arguments in their places (bracketed if they are more than a name: (pcCalc + 1)), and
		#pcCalc, which the expression was all the uses of, goes; the brackets round the expression go with it.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:135:10-135:29,edit:introduceparameter:iScale,printdisklines:Calc.cpp'}),
		[r'^Status: Introduced the parameter int iScale = pcCalc->miScale \+ 7: 1 occurrence replaced, and pcCalc removed\.$',
		 r'^133\|int Area\(int iWidth, int iScale\)$', r'^135\|\treturn iScale \* iWidth;$',
		 r'^155\|\treturn Area\(3, pcCalc->miScale \+ 7\) \+ Area\(i, \(pcCalc \+ 1\)->miScale \+ 7\) \+ Wide\(Next\(&i\)\);$'],
		[r'Problems Detected']),
	('introduce-parameter-keep', dict(MakeIntroduceVariableCopy(), **{
		#With "Remove the parameters only it uses" not ticked, pcCalc stays and the calls pass both.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:135:10-135:29,edit:introduceparameterkeep:iScale,printdisklines:Calc.cpp'}),
		[r'^Status: Introduced the parameter int iScale = pcCalc->miScale \+ 7: 1 occurrence replaced\.$',
		 r'^133\|int Area\(CCalc\* pcCalc, int iWidth, int iScale\)$',
		 r'^155\|\treturn Area\(pcCalc, 3, pcCalc->miScale \+ 7\) \+ Area\(pcCalc \+ 1, i, \(pcCalc \+ 1\)->miScale \+ 7\) \+ Wide\(Next\(&i\)\);$'],
		[]),
	('introduce-parameter-refused', dict(MakeIntroduceVariableCopy(), **{
		#Refused: a call's argument that does something would be worked out twice (iWidth is in the expression twice);
		#and the function changes iBase, so no call can give what iBase * 2 is where it is.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:145:9-145:24,edit:introduceparameter:iSquare,edit:selecttext:122:11-122:20,edit:introduceparameter:iDouble,printdisklines:Calc.cpp'}),
		[r"^Status: Can't introduce it: a call's argument for iWidth \(Next\(&i\)\) does something, and would be worked out more than once\.$",
		 r"^Status: Can't introduce it: the function changes iBase, so a call can't give what the expression is there\.$",
		 r'^118\|int Drift\(int iBase, int iStep\)$', r'^143\|int Wide\(int iWidth\)$'],
		[]),
	('introduce-constant-dialog', dict(MakeIntroduceVariableCopy(), **{
		#The dialog for a kind (Ctrl+Alt+C's): its title and the name it suggests; and the status line says why a
		#selection can't be one.
		'WAIT_INDEX': '1', 'OPEN': 'Calc.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:selecttext:67:19-67:20,introduceconstant,answered:1,printstatus,edit:selecttext:69:29-69:36,introduceconstant,printstatus'}),
		[r'^Dialog \[Introduce Constant\] answered OK$', r'^Status: Introduced the constant VALUE = 4: 1 occurrence replaced\.$',
		 r"^Status: Can't introduce it: the expression isn't a constant: it names a variable or does something\.$"],
		[]),
	('introduce-typedef', dict(MakeIntroduceTypedefCopy(), **{
		#Introduce Typedef: CList<SItem> is written three times in the header.  The typedef goes two blank lines
		#before the class that first writes it (and the comment over that), written as typedefs are here, and each
		#place becomes its name, what was lined up after it staying in its column.
		'WAIT_INDEX': '1', 'OPEN': 'Types.h',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:41:2-41:14,edit:typedef:SItem,edit:typedef:CItemList,printdisklines:Types.h'}),
		[r'^Introduce Typedef: \[CList<SItem>\], 3 places, in the file$', r'^Status: Introduce Typedef: The file uses that name already\.$',
		 r'^Status: Introduced typedef CList<SItem> CItemList: 3 places replaced\.$',
		 r'^34\|\};$', r'^35\|$', r'^36\|$', r'^37\|typedef CList<SItem>\tCItemList;$', r'^38\|$', r'^39\|$', r'^40\|//The items of a thing\.$', r'^41\|class CItems$',
		 r'^44\|\tCItemList\t\t\tmcItems;$', r'^45\|\tunsigned long long\tmulTotal;$', r'^47\|\tCItemList\*\t\t\tGetItems\(void\);$', r'^48\|\tunsigned long long\tTotal\(CItemList\* pcOther\);$'],
		[]),
	('introduce-typedef-keywords', dict(MakeIntroduceTypedefCopy(), **{
		#A type of keywords, in a source file: what a function returns, a variable's type and a cast; the typedef goes
		#before the function that first writes it, over its comment.  The selected place alone can be replaced.
		'WAIT_INDEX': '1', 'OPEN': 'Types.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:20:2-20:20,edit:typedef:uint64,printdisklines:Types.cpp,edit:undo,edit:selecttext:20:2-20:20,edit:typedefone:ulong,printdisklines:Types.cpp'}),
		[r'^Introduce Typedef: \[unsigned long long\], 3 places, in the file$', r'^Status: Introduced typedef unsigned long long uint64: 3 places replaced\.$',
		 r'^11\|\}$', r'^12\|$', r'^13\|$', r'^14\|typedef unsigned long long\tuint64;$', r'^15\|$', r'^16\|$', r'^17\|/{74}$', r'^21\|uint64 CItems::Total\(CList<SItem>\* pcOther\)$',
		 r'^23\|\tuint64\t\t\t\tulResult;$', r'^24\|\tCList<SItem>\*\t\tpcItems;$', r'^27\|\tulResult = \(uint64\)pcItems->miCount;$',
		 r'^Status: Introduced typedef unsigned long long ulong: 1 place replaced\.$', r'^14\|typedef unsigned long long\tulong;$',
		 r'^21\|unsigned long long CItems::Total\(CList<SItem>\* pcOther\)$', r'^23\|\tulong\t\t\t\tulResult;$', r'^27\|\tulResult = \(unsigned long long\)pcItems->miCount;$'],
		[]),
	('introduce-typedef-template', dict(MakeIntroduceTypedefCopy(), **{
		#A type that names a template's parameter is named where the parameter is.  In the class template: a member
		#typedef at the top of the class (question 34), and the places in the class.  In a function of the template
		#outside the class: the function's own, at the start of its body.
		'WAIT_INDEX': '1', 'OPEN': 'Types.h',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:24:2-24:4,edit:typedef:PLast,edit:selecttext:9:2-9:4,edit:typedef:PItem,printdisklines:Types.h'}),
		[r'^Introduce Typedef: \[M\*\], 1 place, in its function$', r'^Status: Introduced typedef M\* PLast: 1 place replaced\.$',
		 r'^Introduce Typedef: \[M\*\], 3 places, in its class$', r'^Status: Introduced typedef M\* PItem: 3 places replaced\.$',
		 r'^8\|public:$', r'^9\|\ttypedef M\*\tPItem;$', r'^10\|\tPItem\tmpFirst;$', r'^11\|\tint\t\tmiCount;$', r'^13\|\tPItem\tGet\(int iIndex\) \{ return mpFirst; \}$', r'^14\|\tPItem\tLast\(void\);$',
		 r'^23\|M\* CList<M>::Last\(void\)$', r'^24\|\{$', r'^25\|\ttypedef M\*\tPLast;$', r'^26\|\tPLast\tpLast;$'],
		[]),
	('introduce-typedef-template-top', dict(MakeIntroduceTypedefCopy(), **{
		#A class template's typedef goes at the top of the class, after the access that starts it, even where the
		#member that first uses it is further down (Last), and a second goes after the first.
		'WAIT_INDEX': '1', 'OPEN': 'Types.h',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:13:2-13:4,edit:typedefone:PLast,edit:selecttext:10:2-10:4,edit:typedefone:PItem,printdisklines:Types.h'}),
		[r'^Status: Introduced typedef M\* PLast: 1 place replaced\.$', r'^Status: Introduced typedef M\* PItem: 1 place replaced\.$',
		 r'^7\|\{$', r'^8\|public:$', r'^9\|\ttypedef M\*\tPLast;$', r'^10\|\ttypedef M\*\tPItem;$', r'^11\|\tPItem\t+mpFirst;$', r'^15\|\tPLast\t+Last\(void\);$'],
		[]),
	('introduce-typedef-files', dict(MakeIntroduceTypedefCopy(), **{
		#A typedef written in a header can name the type in the files that #include the header too (the dialog's
		#tick): Types.cpp, and Report.h and Report.cpp which reach it.  What a macro writes is left.  One undo.
		'WAIT_INDEX': '1', 'OPEN': 'Types.h',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:41:2-41:14,edit:typedeffiles:CItemList,printdisklines:Types.h,printdisklines:Types.cpp,printdisklines:Report.h,printdisklines:Report.cpp,'
				 'edit:undoall,edit:saveall,printdisklines:Report.cpp,printdisklines:Types.h'}),
		[r'^Introduce Typedef: \[CList<SItem>\], 3 places, in the file$', r'^Introduce Typedef: 8 places in 3 files that #include it$',
		 r'^  Report\.cpp: 3 places$', r'^  Types\.cpp: 3 places$', r'^  Report\.h: 2 places$',
		 r'^Status: Introduced typedef CList<SItem> CItemList: 11 places replaced, in 4 files\.$',
		 r'^37\|typedef CList<SItem>\tCItemList;$', r'^44\|\tCItemList\t\t\tmcItems;$', r'^48\|\tunsigned long long\tTotal\(CItemList\* pcOther\);$', r'^52 lines$',
		 r'^8\|CItemList\* CItems::GetItems\(void\)$', r'^21\|\tCItemList\*\t\t\tpcItems;$', r'^26 lines$',
		 r'^6\|#define REPORT_LIST\tCList<SItem>$', r'^9\|int\t\tCountItems\(CItemList\* pcList\);$', r'^10\|int\t\tCountBoth\(CItemList\* pcFirst, REPORT_LIST\* pcSecond\);$', r'^13 lines$',
		 r'^8\|int CountItems\(CItemList\* pcList\)$', r'^20\|\tCItemList\*\t\tpcList;$', r'^24 lines$',
		 r'^8\|int CountItems\(CList<SItem>\* pcList\)$', r'^20\|\tCList<SItem>\*\tpcList;$', r'^24 lines$', r'^41\|\tCList<SItem>\t\tmcItems;$', r'^49 lines$'],
		[]),
	('introduce-typedef-files-dialog', dict(MakeIntroduceTypedefCopy(), **{
		#The dialog: the tick is there for a header, and off: OKed as it is, only the header changes.  Ticked (two
		#tabs from the name, and space), the files are listed first, as Problems Detected's list; Cancel there
		#changes nothing, Continue does it.
		'WAIT_INDEX': '1', 'OPEN': 'Types.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:CItemList,tab,tab,space', 'DIALOG_ANSWER': 'OK|Cancel|OK|OK',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:41:2-41:14,introducetypedef,answered:2,printstatus,printdisklines:Report.h,'
				 'edit:selecttext:41:2-41:14,introducetypedef,answered:4,printstatus,edit:saveall,printdisklines:Report.h'}),
		[r'^Focus: CTextField$', r'^Focus: CCheckBox$', r'^Focus: CCheckBox$', r'^Dialog \[Introduce Typedef\] answered OK$', r'^Dialog \[Problems Detected\] answered Cancel$',
		 r'^Status line: Introduce Typedef: nothing was changed\.$', r'^9\|int\t\tCountItems\(CList<SItem>\* pcList\);$',
		 r'^Dialog \[Introduce Typedef\] answered OK$', r'^Dialog \[Problems Detected\] answered OK$',
		 r'^Status line: Introduced typedef CList<SItem> CItemList: 11 places replaced, in 4 files\.$', r'^9\|int\t\tCountItems\(CItemList\* pcList\);$'],
		[]),
	('introduce-typedef-refused', dict(MakeIntroduceTypedefCopy(), **{
		#What isn't a type: a field's name, an expression.  The dialog (Ctrl+Alt+K's) can't be OKed without a name.
		'WAIT_INDEX': '1', 'OPEN': 'Types.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:selecttext:10:7-10:14,edit:typedefable,edit:selecttext:12:31-12:38,edit:typedefable,edit:selecttext:10:2-10:5,edit:typedefable,edit:selecttext:41:2-41:14,introducetypedef,answered:1,printstatus'}),
		[r"^Introduce Typedef: can't be: the selection isn't a type$", r"^Introduce Typedef: can't be: the selection isn't a type$", r'^Introduce Typedef: \[int\], 3 places, in the file$',
		 r'^Dialog \[Introduce Typedef\] answered Cancel$'],
		[r'^Status: Introduced']),
	('extract-method', dict(MakeExtractMethodCopy(), **{
		#Extract Method: what each variable is to the new function is worked out from how the selection uses it.  Two
		#statements that give iSum its value from the function's parameters: they are parameters, iSum is returned and
		#declared again in the new function, which is const as the one it came from is.  It is declared in a
		#protected part made at the class's end, and defined after the function it came from.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:24:1-25:23,edit:extract:Blend,edit:extract:Scaled,printdisklines:Mix.h,printdisklines:Mix.cpp'}),
		[r'^Extract Method: statements, \[int CMix::X\(int iA, int iB\) const\], called \[X\(iA, iB\)\]$', r'^  iA: a parameter$', r'^  iB: a parameter$', r'^  iSum: returned$',
		 r'^Status: Extract Method: The file uses that name already\.$', r'^Status: Extracted int CMix::Scaled\(int iA, int iB\) const\.$',
		 r'^25\|\tint\t\tTwice\(int iA\) \{ return iA \* 2 \+ miBase; \}$', r'^26\|$', r'^27\|protected:$', r'^28\|\tint\t\tScaled\(int iA, int iB\) const;$', r'^29\|\};$',
		 r'^19\|int CMix::Blend\(int iA, int iB\) const$', r'^21\|\tint\tiSum;$', r'^22\|\tint\tiResult;$', r'^23\|$', r'^24\|\tiSum = Scaled\(iA, iB\);$', r'^25\|\tiResult = iSum \* 2;$', r'^27\|\}$',
		 r'^28\|$', r'^29\|$', r'^30\|/{74}$', r'^34\|int CMix::Scaled\(int iA, int iB\) const$', r'^35\|\{$', r'^36\|\tint\tiSum;$', r'^37\|$', r'^38\|\tiSum = iA \+ iB;$',
		 r'^39\|\tiSum = iSum \* miBase;$', r'^40\|\treturn iSum;$', r'^41\|\}$', r'^42\|$', r'^43\|$', r'^44\|/{74}$', r'^48\|int CMix::Sum\(int iLimit\)$'],
		[]),
	('extract-method-flow', dict(MakeExtractMethodCopy(), **{
		#The other ways variables go: a loop's own counter moves into the new function with its declaration; two
		#variables used afterwards are reference parameters; an object is given by reference, never copied; and a
		#selection that ends by returning from the function becomes "return Name(...)".
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:89:1-90:26,edit:extract:Swapped,edit:selecttext:73:1-77:15,edit:extract:Capped,edit:selecttext:58:1-59:17,edit:extract:Around,edit:selecttext:40:1-44:3,edit:extract:Totalled,printdisklines:Mix.h,printdisklines:Mix.cpp'}),
		[r'^Extract Method: statements, \[void CMix::X\(SPoint\* psPoint, SPoint& sOther\)\], called \[X\(psPoint, sOther\)\]$', r'^  sOther: a reference parameter$',
		 r'^Extract Method: statements that return, \[int CMix::X\(int iHalf\)\], called \[X\(iHalf\)\]$',
		 r'^Extract Method: statements, \[void CMix::X\(int iA, int& iLow, int& iHigh\)\], called \[X\(iA, iLow, iHigh\)\]$', r'^  iLow: a reference parameter$',
		 r'^Extract Method: statements, \[int CMix::X\(int iLimit\)\], called \[X\(iLimit\)\]$', r'^  i: moved in$', r'^  iTotal: returned$',
		 r'^27\|protected:$', r'^28\|\tvoid\tSwapped\(SPoint\* psPoint, SPoint& sOther\);$', r'^29\|\tint\t\tCapped\(int iHalf\);$', r'^30\|\tvoid\tAround\(int iA, int& iLow, int& iHigh\);$',
		 r'^31\|\tint\t\tTotalled\(int iLimit\);$',
		 r'^\d+\|int CMix::Sum\(int iLimit\)$', r'^\d+\|\{$', r'^\d+\|\tint\tiTotal;$', r'^\d+\|$', r'^\d+\|\tiTotal = Totalled\(iLimit\);$', r'^\d+\|\treturn iTotal;$',
		 r'^\d+\|int CMix::Totalled\(int iLimit\)$', r'^\d+\|\{$', r'^\d+\|\tint\ti;$', r'^\d+\|\tint\tiTotal;$', r'^\d+\|$', r'^\d+\|\tiTotal = 0;$', r'^\d+\|\tfor \(i = 0; i < iLimit; i\+\+\)$',
		 r'^\d+\|\t\tiTotal \+= i \* miBase;$', r'^\d+\|\treturn iTotal;$',
		 r'^\d+\|\tAround\(iA, iLow, iHigh\);$', r'^\d+\|\tmiCount = iLow \* iHigh;$', r'^\d+\|void CMix::Around\(int iA, int& iLow, int& iHigh\)$', r'^\d+\|\tiLow = iA - 1;$',
		 r'^\d+\|\tiHalf = iA / 2;$', r'^\d+\|\treturn Capped\(iHalf\);$', r'^\d+\|int CMix::Capped\(int iHalf\)$', r'^\d+\|\tif \(iHalf > miBase\)$', r'^\d+\|\t\treturn miBase;$', r'^\d+\|\treturn iHalf;$',
		 r'^\d+\|\tSPoint\tsOther;$', r'^\d+\|\tSwapped\(psPoint, sOther\);$', r'^\d+\|\tpsPoint->iX = sOther\.iY;$', r'^\d+\|void CMix::Swapped\(SPoint\* psPoint, SPoint& sOther\)$',
		 r'^\d+\|\tsOther\.iX = psPoint->iX \+ miBase;$'],
		[]),
	('extract-method-pointers', dict(MakeExtractMethodCopy(), **{
		#With the dialog's "by pointer" asked for, what the new function would be given by reference it is given by
		#pointer, named p and its name: the statements' uses are a pointer's and the call gives addresses.  An object
		#is a pointer too.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:89:1-90:26,edit:extractpointers:Swapped,edit:selecttext:58:1-59:17,edit:extractpointers:Around,printdisklines:Mix.h,printdisklines:Mix.cpp'}),
		[r'^Status: Extracted void CMix::Swapped\(SPoint\* psPoint, SPoint\* psOther\)\.$', r'^Status: Extracted void CMix::Around\(int iA, int\* piLow, int\* piHigh\)\.$',
		 r'^28\|\tvoid\tSwapped\(SPoint\* psPoint, SPoint\* psOther\);$', r'^29\|\tvoid\tAround\(int iA, int\* piLow, int\* piHigh\);$',
		 r'^58\|\tAround\(iA, &iLow, &iHigh\);$', r'^59\|\tmiCount = iLow \* iHigh;$', r'^67\|void CMix::Around\(int iA, int\* piLow, int\* piHigh\)$', r'^69\|\t\*piLow = iA - 1;$',
		 r'^70\|\t\*piHigh = iA \+ 1;$', r'^99\|\tSwapped\(psPoint, &sOther\);$', r'^100\|\tpsPoint->iX = sOther\.iY;$', r'^109\|void CMix::Swapped\(SPoint\* psPoint, SPoint\* psOther\)$',
		 r'^111\|\tpsOther->iX = psPoint->iX \+ miBase;$', r'^112\|\tpsOther->iY = psPoint->iY;$'],
		[]),
	('extract-method-static', dict(MakeExtractMethodCopy(), **{
		#Static, where the selection uses nothing of its object: declared static in the class, and not const.  Where
		#it uses a field it can't be, and is as it would be without.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:58:1-59:17,edit:extractstatic:Around,edit:selecttext:24:1-25:23,edit:extractstatic:Scaled,printdisklines:Mix.h,printdisklines:Mix.cpp'}),
		[r'^  it can be static$', r'^Status: Extracted void CMix::Around\(int iA, int& iLow, int& iHigh\)\.$',
		 r'^Extract Method: statements, \[int CMix::X\(int iA, int iB\) const\], called \[X\(iA, iB\)\]$', r'^Status: Extracted int CMix::Scaled\(int iA, int iB\) const\.$',
		 r'^28\|\tstatic void\tAround\(int iA, int& iLow, int& iHigh\);$', r'^29\|\tint\t\t\tScaled\(int iA, int iB\) const;$',
		 r'^34\|int CMix::Scaled\(int iA, int iB\) const$', r'^71\|\tAround\(iA, iLow, iHigh\);$', r'^80\|void CMix::Around\(int iA, int& iLow, int& iHigh\)$'],
		[r'^  it can be static\n(.*\n)*  it can be static']),
	('extract-method-duplicates', dict(MakeExtractMethodCopy(), **{
		#The other places in the file the selection is written, its variables' names apart, are found and made calls
		#of the new function too, with their own names: Second's two statements are First's with other names; Third's
		#subtract, and aren't.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:129:1-130:18,edit:extractduplicates:Tripled,printdisklines:Mix.cpp'}),
		[r'^Duplicates: line 144 in Second$', r'^Status: Extracted int Tripled\(int iA, int iB\)\.  1 other place it was written is a call of it too\.$',
		 r'^125\|int Tripled\(int iA, int iB\)$', r'^129\|\tiSum = iA \+ iB;$', r'^130\|\tiSum = iSum \* 3;$', r'^131\|\treturn iSum;$',
		 r'^143\|\tiSum = Tripled\(iA, iB\);$', r'^157\|\tiAll = Tripled\(iX, iY\);$', r'^158\|\tiMore = iAll \+ 1;$', r'^171\|\tiAll = iX - iY;$', r'^172\|\tiAll = iAll \* 3;$',
		 r'^174 lines$'],
		[]),
	('extract-method-dialog', dict(MakeExtractMethodCopy(), **{
		#The dialog: the name typed.  Where the selection is written again, Duplicates Detected lists the places,
		#ticked, and OK makes them calls too.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:Made', 'DIALOG_ANSWER': 'OK|OK',
		'DEBUG': 'edit:selecttext:129:1-130:18,extractmethod,answered:2,printstatus'}),
		[r'^Dialog \[Extract Method\] answered OK$', r'^Dialog \[Duplicates Detected\] answered OK$',
		 r'^Status: Extracted int Made\(int iA, int iB\)\.  1 other place it was written is a call of it too\.$'],
		[]),
	('find-duplicates-function', dict(MakeFindDuplicatesCopy(), **{
		#With nothing selected, the function the caret is in (CLedger::Add): the places its whole body is written, its
		#variables and parameters with other names (CBook::Merge).
		'WAIT_INDEX': '1', 'OPEN': 'Ledger.cpp',
		'DEBUG': 'edit:caret:15:2,edit:duplicates'}),
		[r'^Status: Find Duplicates: 1 place found\.$', r'^Duplicates of CLedger::Add in the solution  1 place, 1 part$',
		 r'^  The whole body, 5 statements, 22 tokens  1 place$', r'^    Book\.cpp  1 place$', r'^      6: int iJoined;$'],
		[r'Ledger\.cpp  ']),
	('find-duplicates-runs', dict(MakeFindDuplicatesCopy(), **{
		#A function whose body isn't written whole anywhere (CLedger::Tally): the run of its first three statements is
		#(CBook::Recount's, which does more after them), listed once, not again for the loop's block inside it.  With
		#a minimum it is bigger than, nothing.
		'WAIT_INDEX': '1', 'OPEN': 'Ledger.cpp',
		'DEBUG': 'edit:caret:27:2,edit:duplicates,edit:duplicatesize:30:200,edit:duplicates'}),
		[r'^Status: Find Duplicates: 1 place found\.$', r'^Duplicates of CLedger::Tally in the solution  1 place, 1 part$',
		 r'^  Lines 24 to 33, 7 statements, 42 tokens  1 place$', r'^    Book\.cpp  1 place$', r'^      17: int j;$',
		 r'^Status: Find Duplicates: no duplicates found\.$', r'^Duplicates of CLedger::Tally in the solution  0 places, 0 parts$'],
		[r'iEvery \+= psItem', r'The whole body']),
	('find-duplicates-selected', dict(MakeFindDuplicatesCopy(), **{
		#Selected statements (Add's first two) are found as a run of statements; a selected expression (Distance's) is
		#found inside another statement (Scaled's assignment).  In the current file only, neither is.
		'WAIT_INDEX': '1', 'OPEN': 'Ledger.cpp',
		'DEBUG': 'edit:selecttext:15:2-16:23,edit:duplicates,edit:selecttext:40:9-40:26,edit:duplicates,edit:duplicatescope:4,edit:duplicates'}),
		[r'^Duplicates of the statements at line 15 to 16 in the solution  1 place$', r'^  Book\.cpp  1 place$', r'^    8: iJoined = iP \+ iQ;$',
		 r"^Duplicates of 'iX \* iX \+ iY \* iY' in the solution  1 place$", r'^    36: iLength = iX \* iX \+ iY \* iY;$',
		 r"^Duplicates of 'iX \* iX \+ iY \* iY' in Ledger\.cpp  0 places$"],
		[]),
	('find-duplicates-literals', dict(MakeFindDuplicatesCopy(), **{
		#Literals must be the same (Offset's iA * 4 + 16 isn't Shift's iZ * 8 + 32), unless they may differ, when a
		#number is any number; a name is never a literal (Kept's iA * 4 + miRate isn't found either way).
		'WAIT_INDEX': '1', 'OPEN': 'Ledger.cpp',
		'DEBUG': 'edit:selecttext:46:9-46:20,edit:duplicates,edit:duplicatescope:0:l,edit:duplicates'}),
		[r'^Status: Find Duplicates: no duplicates found\.$', r"^Duplicates of 'iA \* 4 \+ 16' in the solution  0 places$",
		 r"^Duplicates of 'iA \* 4 \+ 16' in the solution  1 place$", r'^    43: return iZ \* 8 \+ 32;$'],
		[r'miRate;$']),
	('find-duplicates-settings', dict(MakeFindDuplicatesCopy(), **{
		#Settings' Find Duplicates page: the minimums and the tick, kept by OK and written with the settings.
		'SETTINGS': MakeSettingsFile(), 'DUPLICATES_EDITS': 'statements:4;tokens:12;literals:on', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:printduplicatesettings,clickmenu:File/Settings...,answered:1,edit:printduplicatesettings'}),
		[r'^Duplicates settings: Solution, literals the same, at least 3 statements and 20 tokens$', r'^Dialog \[Settings\] answered OK$',
		 r'^Duplicates settings: Solution, literals may differ, at least 4 statements and 12 tokens$'],
		[]),
	('find-duplicates-dialog', dict(MakeFindDuplicatesCopy(), **{
		#Control+Alt+Shift+D on the Edit menu: the dialog says what it looks for, and Find lists what it found in the
		#Duplicates tool window; its first place opens with a click.
		'WAIT_INDEX': '1', 'OPEN': 'Ledger.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:caret:15:2,command:Edit.FindDuplicates,answered:1,wait:2000,edit:printduplicates,edit:duplicateopen,printtabs'}),
		[r'^Dialog \[Find Duplicates\] answered OK$', r'^Duplicates of CLedger::Add in the solution  1 place, 1 part$', r'^      6: int iJoined;$',
		 r'^Tabs: Ledger\.cpp >Book\.cpp$'],
		[]),
	('extract-method-template', dict(MakeExtractMethodCopy(), **{
		#In templates (T4).  From a function template the new function is a template of the same parameters, defined
		#before it, and the call names them.  A class template's member defined outside the class gets one defined the
		#same way, under the class's template<...>, and declared in the class.  And an expression in a function defined
		#in its class becomes a function defined there, on one line as that one is.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.h',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:64:1-68:3,edit:extract:Chosen,edit:selecttext:49:1-50:23,edit:extract:Weighed,edit:selecttext:25:30-25:45,edit:extract:Doubled,printdisklines:Mix.h'}),
		[r'^Extract Method: statements, \[M X\(M a, M b\)\], called \[X<M>\(a, b\)\]$', r'^  cResult: returned$',
		 r'^Extract Method: statements, \[int CPile<M>::X\(int iScale\)\], called \[X\(iScale\)\]$', r'^Extract Method: an expression, \[int X\(int iA\)\], called \[X\(iA\)\]$',
		 r'^25\|\tint\t\tTwice\(int iA\) \{ return Doubled\(iA\); \}$', r'^26\|\tint\t\tDoubled\(int iA\) \{ return iA \* 2 \+ miBase; \}$', r'^27\|\};$',
		 r'^37\|\tint\t\tWeigh\(int iScale\);$', r'^38\|$', r'^39\|protected:$', r'^40\|\tint\t\tWeighed\(int iScale\);$', r'^41\|\};$',
		 r'^\d+\|template<class M>$', r'^\d+\|int CPile<M>::Weigh\(int iScale\)$', r'^\d+\|\tiWeight = Weighed\(iScale\);$', r'^\d+\|\treturn iWeight;$',
		 r'^\d+\|template<class M>$', r'^\d+\|int CPile<M>::Weighed\(int iScale\)$', r'^\d+\|\tint\tiWeight;$', r'^\d+\|\tiWeight = miDepth \* iScale;$', r'^\d+\|\tiWeight \+= sizeof\(M\);$', r'^\d+\|\treturn iWeight;$',
		 r'^\d+\|template<class M>$', r'^\d+\|M Chosen\(M a, M b\)$', r'^\d+\|\tM\tcResult;$', r'^\d+\|\tcResult = a;$', r'^\d+\|\treturn cResult;$',
		 r'^\d+\|template<class M>$', r'^\d+\|M Larger\(M a, M b\)$', r'^\d+\|\tcResult = Chosen<M>\(a, b\);$', r'^\d+\|\treturn cResult;$'],
		[]),
	('extract-method-refused', dict(MakeExtractMethodCopy(), **{
		#What can't be a function: statements that return part of the way through; ones a break leaves; a part of a
		#statement that isn't an expression.  The dialog (Ctrl+Alt+M's) shows the function it would be.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:selecttext:73:1-76:3,edit:extractable,edit:selecttext:110:1-114:4,edit:extractable,edit:selecttext:110:3-110:20,edit:extractable,edit:selecttext:24:1-25:23,extractmethod,answered:1,printstatus'}),
		[r"^Extract Method: can't be: it returns from the function part of the way through: a selection that returns must end with a return \(or a throw\)$",
		 r"^Extract Method: can't be: it leaves by a break, a continue or a goto that a function of it couldn't make \(or has a label\)$",
		 r"^Extract Method: can't be: the selection isn't an expression, or whole statements of one block$", r'^Dialog \[Extract Method\] answered Cancel$'],
		[r'^Status: Extracted']),
	('exceptions-check-options', dict(MakeExceptionsCopy(), **{
		#The code model checks a file with its build configuration's compile options, made by the code the build makes
		#them with (CBuildConfiguration::SetCompileOptions; ExceptionsPlan.txt 10a, question 17): with Exceptions="true"
		#the throws are no errors.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp', 'DEBUG': 'edit:printcheckerrors'}),
		[r'^Check errors: 0$'],
		[r"can't be used"]),
	('exceptions-check-options-off', dict(MakeExceptionsCopy(False), **{
		#The same solution built without exceptions: each throw is the error the build reports.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp', 'DEBUG': 'edit:printcheckerrors'}),
		[r'^Check errors: 3$', r"^  line 20: throw can't be used: the program is built without exceptions", r"^  line 49: throw can't be used", r"^  line 88: throw can't be used"],
		[]),
	('exceptions-usages-throw', dict(MakeExceptionsCopy(), **{
		#A program built with exceptions is read as it is built (ExceptionsPlan.txt 10a): a throw's operand is checked,
		#so what it names are usages.  iLimit is used in Check's throw, and CFault is named in two throws, a handler and
		#a throw(CFault).
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp', 'CARET': '16:15', 'EDIT': 'usages'}),
		[r'^Usages of iLimit  3 usages$', r'^\s+20: throw CFault\(iLimit\);$'],
		[]),
	('exceptions-usages-class', dict(MakeExceptionsCopy(), **{
		#CFault, named in throws, a handler's type and throw(CFault) lists: each is a usage.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.h', 'CARET': '5:8', 'EDIT': 'usages'}),
		[r'^Usages of CFault  7 usages$', r'^\s+34: catch \(CFault& cFault\)$', r'^\s+16: int Check\(int iLimit\) throw\(CFault\)$',
		 r'^\s+15: int Check\(int iLimit\) throw\(CFault\);$', r'^\s+20: throw CFault\(iLimit\);$', r'^\s+49: throw CFault\(iValue\);$'],
		[]),
	('exceptions-rename-handler', dict(MakeExceptionsCopy(), **{
		#A handler's parameter is a local of the handler: renamed where it is declared and where it is used.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp', 'CARET': '34:18',
		'DEBUG': 'edit:renamesearch:---s,edit:rename:cError,printdisklines:Fault.cpp'}),
		[r'^34\|\tcatch \(CFault& cError\)$', r'^36\|\t\tiResult = cError\.GetCode\(\) \+ 1;$'],
		[r'^\d+\|.*cFault']),
	('exceptions-extract-method', dict(MakeExceptionsCopy(), **{
		#Extract Method (ExceptionsPlan.txt 10b): statements that return and end with a throw leave on every path, so
		#they are extracted and called as return Picked(iValue);.  Statements in a handler that use its parameter are
		#given it, as a local is.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:45:1-49:23,edit:extract:Picked,edit:selecttext:36:1-37:25,edit:extract:Recovered,printdisklines:Fault.cpp'}),
		[r'^Extract Method: statements that return, \[int X\(int iValue\)\], called \[X\(iValue\)\]$', r'^Status: Extracted int Picked\(int iValue\)\.$',
		 r'^Extract Method: statements, \[int X\(CFault& cFault\)\], called \[X\(cFault\)\]$', r'^  cFault: a parameter$',
		 r'^Status: Extracted int Recovered\(CFault& cFault\)\.$',
		 r'^\d+\|int Recovered\(CFault& cFault\)$', r'^\d+\|\tiResult = cFault\.GetCode\(\) \+ 1;$', r'^\d+\|\tcatch \(CFault& cFault\)$',
		 r'^\d+\|\t\tiResult = Recovered\(cFault\);$', r'^\d+\|int Picked\(int iValue\)$', r'^\d+\|\tthrow CFault\(iValue\);$', r'^\d+\|\treturn Picked\(iValue\);$'],
		[r"can't be"]),
	('exceptions-inline-refused', dict(MakeExceptionsCopy(), **{
		#Inline Function (ExceptionsPlan.txt 10c): a function-try-block can't be inlined, nor a noexcept function whose
		#body may throw; a noexcept one whose body can't (Safe) can.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:53:6,edit:inlinablefunction,edit:caret:64:6,edit:inlinablefunction,edit:caret:70:6,edit:inlinablefunction'}),
		[r"^Inline Function: Wrapped can't be inlined: its body is a function-try-block: its handlers can't be written where it is called$",
		 r"^Inline Function: Promise can't be inlined: it has an exception specification \(noexcept or throw\(\)\) and its body may throw: inlined, an exception would no longer stop at it$",
		 r'^Inline Function: Safe, an expression, 3 calls$', r'^  Fault\.cpp line 99: in an expression$'],
		[r"Safe can't be"]),
	('exceptions-change-signature', dict(MakeExceptionsCopy(), **{
		#Change Signature (ExceptionsPlan.txt 10e) keeps what comes after the parameters: throw(CFault) on Check's
		#declaration and definition.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:15:6,edit:changesignature:0;+int|iBase|0|,printdisklines:Fault.h,printdisklines:Fault.cpp'}),
		[r'^15\|int\tCheck\(int iLimit, int iBase\) throw\(CFault\);$', r'^16\|int Check\(int iLimit, int iBase\) throw\(CFault\)$', r'^\d+\|\t\tiResult = Check\(iValue, 0\);$'],
		[r'^Conflict']),
	('exceptions-introduce-variable', dict(MakeExceptionsCopy(), **{
		#Introduce Variable inside a handler, on an expression that uses the handler's parameter: declared with the
		#function's declarations and given its value in the handler, before the statement that used it.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:selecttext:36:13-36:29,edit:introduce:iCode,printdisklines:Fault.cpp'}),
		[r'^Status: Introduced int iCode = cFault\.GetCode\(\): 1 occurrence replaced\.$', r'^\d+\|\tint\tiCode;$', r'^35\|\tcatch \(CFault& cFault\)$',
		 r'^37\|\t\tiCode = cFault\.GetCode\(\);$', r'^38\|\t\tiResult = iCode \+ 1;$'],
		[]),
	('exceptions-usages-noexcept', dict(MakeExceptionsCopy(), **{
		#A function named in noexcept(e), which isn't run, is still a usage of it (ExceptionsPlan.txt 10d).
		'WAIT_INDEX': '1', 'OPEN': 'Fault.h', 'CARET': '20:6', 'EDIT': 'usages'}),
		[r'^Usages of Safe  ', r'^\s+99: int Relayed\(int iValue\) noexcept\(noexcept\(Safe\(iValue\)\)\)$', r'^\s+101: return Safe\(iValue\);$'],
		[]),
	('exceptions-safe-delete', dict(MakeExceptionsCopy(), **{
		#Safe Delete (ExceptionsPlan.txt 10e): a class that is only thrown and caught is used: its throw and its handler
		#are its usages.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.h', 'CARET': '25:8', 'DEBUG': 'edit:safedelete'}),
		[r'^Safe Delete: CTimeout \[CTimeout\]', r'^  used at Fault\.cpp 88:10$', r'^  used at Fault\.cpp 91:9$', r'^Safe Delete: not deleted, it is used$'],
		[r'^Status: Deleted']),
	('exceptions-move-function', dict(MakeExceptionsCopy(), **{
		#Move to File (ExceptionsPlan.txt 10e) on a function whose body is a function-try-block: its definition goes
		#whole, the handler too, and the declaration with it.
		'WAIT_INDEX': '1', 'OPEN': 'Fault.h', 'CARET': '18:6',
		'DEBUG': 'edit:renamesearch:---s,edit:movefunction:Other.h,printdisklines:Other.h,printdisklines:Other.cpp,printdisklines:Fault.cpp'}),
		[r'^Status: Moved Wrapped to Other\.h and Other\.cpp', r'^Disk lines Other\.h:$', r'^\d+\|int\tWrapped\(int iValue\);$',
		 r'^Disk lines Other\.cpp:$', r'^\d+\|int Wrapped\(int iValue\)$', r'^\d+\|try$', r'^\d+\|\treturn Check\(iValue\);$', r'^\d+\|catch \(\.\.\.\)$', r'^\d+\|\treturn 0;$',
		 r'^Disk lines Fault\.cpp:$'],
		[r'^Move to File: can']),
	('inline-function', dict(MakeInlineFunctionCopy(), **{
		#Inline Function, for functions that return an expression: every call becomes the expression, each parameter
		#the call's argument (in parentheses where it is more than one thing and an operand), the whole in parentheses
		#where what it is written into would take it apart, and a member named alone gets the call's object before it.
		#The functions are taken out, their declarations too.
		'WAIT_INDEX': '1', 'OPEN': 'Pump.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:37:7,edit:inlinefunction:all,edit:open:Pump.h,edit:caret:27:9,edit:inlinefunction:all,printdisklines:Pump.h,printdisklines:Pump.cpp'}),
		[r'^Inline Function: Twice, an expression, 2 calls$', r'^  Pump\.cpp line 127: in an expression$', r'^  Pump\.cpp line 132: returned$', r'^Status: Inlined Twice: 2 calls; the function is taken out\.$',
		 r'^Inline Function: CPump::Scaled, an expression, 2 calls$', r'^  Pump\.cpp line 41: in an expression$', r'^  Pump\.cpp line 118: in an expression, on pcPump->, outside the class$',
		 r'^Status: Inlined CPump::Scaled: 2 calls; the function is taken out\.$',
		 r'^26\|\tint\t\t\tGetRate\(void\) \{ return miRate; \}$', r'^27\|\tvoid\t\tStep\(int iBy\);$', r'^36\|void\tReport\(CPump\* pcPump, int iWhat\);$', r'^39 lines$',
		 r'^41\|\treturn miCount \+ \(miLimit \* \(i \+ 1\)\);$', r'^117\|\tiTotal = \(\(pcPump->GetRate\(\) \+ 1\) \* 2\) \* 2;$', r'^118\|\tiTotal \+= pcPump->miLimit \* \(iTotal - 1\);$',
		 r'^122\|\treturn iTotal \* 2;$', r'^145 lines$'],
		[r'^Conflict', r'Twice\(', r'Scaled\(']),
	('inline-function-statements', dict(MakeInlineFunctionCopy(), **{
		#Functions with statements go where their call is a statement.  Report's statements are written at its call,
		#its parameter (called on) the argument in parentheses; Step's too, in a loop of another member (its own
		#variable's declaration joins that function's, lined up with them) and, since the conflict is continued past,
		#on an object outside the class.  A class template's member inlined outside the template has the template's
		#parameter replaced by the instance's argument, as the object's declaration writes it.
		'WAIT_INDEX': '1', 'OPEN': 'Pump.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:38:7,edit:inlinefunction:all,edit:open:Pump.h,edit:caret:12:8,edit:inlinefunction:keep,edit:open:Pump.h,edit:caret:28:9,edit:inlinefunction:keep,'
		         'edit:inlinefunction:keep:anyway,printdisklines:Pump.h,printdisklines:Pump.cpp'}),
		[r'^Inline Function: Report, statements, 1 call$', r'^Status: Inlined Report: 1 call; the function is taken out\.$',
		 r'^Inline Function: CSlot::Bytes, an expression, 1 call$', r'^  Pump\.cpp line 121: assigned, on cSlot\., outside the class$', r'^Status: Inlined CSlot::Bytes: 1 call\.$',
		 r'^Inline Function: CPump::Step, statements, 2 calls$', r"^Conflict: The body names miCount, which isn't public: the call at Pump\.cpp line 115 is outside the class\.$",
		 r'^Status: Inlined CPump::Step: 2 calls\.$',
		 r'^12\|\tint\t\tBytes\(void\) \{ return miUsed \* sizeof\(M\); \}$', r'^28\|\tvoid\t\tStep\(int iBy\);$', r'^37\|int\t\tTwice\(int iValue\);$', r'^40 lines$',
		 r'^35\|\tint\ti;$', r'^36\|\tint\tiNext;$', r'^37\|$', r'^38\|\tfor \(i = 0; i < iTimes; i\+\+\)$', r'^40\|\t\tiNext = miCount \+ \(i \+ 1\);$', r'^41\|\t\tmiCount = iNext;$', r'^42\|\t\}$',
		 r'^114\|\tint\t\t\tiTotal;$', r'^115\|\tint\t\t\tiNext;$', r'^116\|$', r'^117\|\tcOther\.Init\(3\);$', r'^118\|\tiNext = cOther\.miCount \+ 2;$', r'^119\|\tcOther\.miCount = iNext;$',
		 r'^122\|\tcOther\.Kind\(\);$', r'^123\|\tcOther\.miLimit = iTotal;$', r'^125\|\tiTotal = cSlot\.miUsed \* sizeof\(long\);$', r'^149 lines$'],
		[r'Report\(']),
	('inline-function-refused', dict(MakeInlineFunctionCopy(), **{
		#What can't be inlined: a virtual function, one that returns part of the way through.  A member that isn't public named where the call is outside the class is a conflict: nothing is
		#inlined.  And one call alone, at the caret: the function stays.
		'WAIT_INDEX': '1', 'OPEN': 'Pump.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:30:15,edit:inlinablefunction,edit:caret:31:9,edit:inlinablefunction,edit:caret:26:9,edit:inlinefunction:all,'
		         'edit:open:Pump.cpp,edit:caret:132:10,edit:inlinefunction:this,printdisklines:Pump.h,printdisklines:Pump.cpp'}),
		[r"^Inline Function: CPump::Kind can't be inlined: it is virtual$",
		 r"^Inline Function: CPump::Loop can't be inlined: it returns part of the way through its body: only a function that returns once, at its end, is inlined$",
		 r"^Conflict: The body names miRate, which isn't public: the call at Pump\.cpp line 127 is outside the class\.$",
		 r'^  Pump\.cpp line 132: returned, at the caret$', r'^Status: Inlined Twice: 1 call\.$',
		 r'^26\|\tint\t\t\tGetRate\(void\) \{ return miRate; \}$', r'^37\|int\t\tTwice\(int iValue\);$',
		 r'^127\|\tiTotal = Twice\(pcPump->GetRate\(\) \+ 1\) \* 2;$', r'^132\|\treturn iTotal \* 2;$', r'^155 lines$'],
		[r'Inlined CPump::GetRate']),
	('inline-function-temporaries', dict(MakeInlineFunctionCopy(), **{
		#A function that changes its parameter is inlined with a variable for it (numbered where the function it
		#goes into has the name), and a call inside a bigger expression is taken out of it: the statements before
		#the statement, what it returned in a variable.  A loop's condition and an arm of ?: keep their calls, so
		#the function stays.
		'WAIT_INDEX': '1', 'OPEN': 'Pump.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:33:9,edit:inlinablefunction,edit:inlinefunction:all:anyway,printdisklines:Pump.h,printdisklines:Pump.cpp'}),
		[r'^Inline Function: CPump::Bump, statements, 6 calls$', r'^  its parameter iA is changed: it gets a variable$',
		 r'^  Pump\.cpp line 144: assigned, on pcPump->, outside the class$', r'^  Pump\.cpp line 145: taken out of an expression, on pcPump->, outside the class$',
		 r'^  Pump\.cpp line 146: taken out of an expression, on pcPump->, outside the class$', r'^  Pump\.cpp line 146: taken out of an expression, on pcPump->, outside the class$',
		 r'^  Pump\.cpp line 150: in an expression, on pcPump->, outside the class$', r'^  Pump\.cpp line 154: in an expression, on pcPump->, outside the class$',
		 r"^Conflict: The call at Pump\.cpp line 150 is part of an expression it can't be taken out of, and Bump has statements: it stays a call\.$",
		 r'^Status: Inlined CPump::Bump: 4 calls, 2 left as they were\.$',
		 r'^33\|\tint\t\t\tBump\(int iA\);$',
		 r'^142\|\tint\tiTotal;$', r'^143\|\tint\tiA2;$', r'^144\|\tint\tiBump;$', r'^145\|\tint\tiBump2;$', r'^146\|\tint\tiBump3;$', r'^147\|$',
		 r'^148\|\tiA2 = 3;$', r'^149\|\tiA2 = iA2 \+ 1;$', r'^150\|\tiTotal = iA2;$', r'^151\|\tiA2 = iA;$', r'^152\|\tiA2 = iA2 \+ 1;$', r'^153\|\tiBump = iA2;$',
		 r'^154\|\tiTotal = iTotal \+ iBump \* 2;$', r'^157\|\tiBump2 = iA2;$', r'^158\|\tiA2 = 4;$', r'^160\|\tiBump3 = iA2;$', r'^161\|\tif \(iBump2 > iBump3\)$',
		 r'^165\|\twhile \(pcPump->Bump\(1\) < iTotal\)$', r'^169\|\treturn \(iTotal > 0\) \? pcPump->Bump\(iTotal\) : 0;$', r'^170 lines$'],
		[]),
	('inline-function-dialog', dict(MakeInlineFunctionCopy(), **{
		#Ctrl+Alt+N on a function: the dialog asks which calls, then Problems Detected if anything is in the way;
		#on something that is neither a variable nor a function the status line says so.
		'WAIT_INDEX': '1', 'OPEN': 'Pump.h', 'CARET': '26:9', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|Cancel',
		'DEBUG': 'inlinevariable,answered:2,printstatus,edit:caret:30:15,inlinevariable,printstatus,edit:caret:16:8,inlinevariable,printstatus'}),
		[r'^Dialog \[Inline CPump::GetRate\] answered OK$', r'^Dialog \[Problems Detected\] answered Cancel$', r"^Status: Can't inline CPump::Kind: it is virtual\.$",
		 r'^Status: There is no variable or function at the caret to inline\.$'],
		[r'^Status: Inlined']),
	('extract-superclass', dict(MakeMoveMembersCopy(), **{
		#Extract Superclass: a field and two functions of CDrum become CBarrel, in Barrel.h (a guard, Drum.h's
		#includes, each under its access, with its comment) and Barrel.cpp (the function defined outside, renamed);
		#Roll is only declared there, pure virtual, and stays CDrum's; CDrum derives from CBarrel too and its header
		#includes Barrel.h; the files are in the project.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:extractsuperclass:CKeg|Fill(int),edit:extractsuperclass:CBarrel|Fill(int),edit:extractsuperclass:CBarrel|miLevel;GetLevel();Fill(int)|Roll(),'
		         'printdisklines:Barrel.h,printdisklines:Barrel.cpp,printdisklines:Drum.h,printdisklines:Drum.cpp,printdisklines:MoveMembers.vcxproj'}),
		[r'^Extract Class: there is a CKeg already$', r"^Conflict: Fill\(int\) uses CDrum::miLevel, which CBarrel doesn't have\.$", r'^Extract Class: not extracted$',
		 r'^Status: Extracted the superclass CBarrel from CDrum: 4 members, in Barrel\.h and Barrel\.cpp\.$',
		 r'^1\|#ifndef __BARREL_H__$', r'^3\|#include "Vessel\.h"$', r'^4\|$', r'^5\|$', r'^6\|class CBarrel$', r'^7\|\{$', r'^8\|protected:$', r'^9\|\tint\t\tmiLevel;$', r'^10\|$', r'^11\|public:$',
		 r'^12\|\t//How full it is\.$', r'^13\|\tint\t\tGetLevel\(void\) \{ return miLevel; \}$', r'^14\|\tvoid\tFill\(int iAmount\);$', r'^15\|\tvirtual void\tRoll\(void\) =0;$', r'^16\|\};$', r'^19\|#endif // __BARREL_H__$',
		 r'^1\|#include "Barrel\.h"$', r'^2\|#include "Drum\.h"$', r'^9\|void CBarrel::Fill\(int iAmount\)$', r'^11\|\tmiLevel \+= iAmount;$', r'^12 lines$',
		 r'^4\|#include "Barrel\.h"$', r'^7\|class CDrum : public CVessel, public CBarrel$', r'^13\|\tint\t\tmiRate, miStep;$', r'^16\|\tvoid\tInit\(int iVolume\);$', r'^17\|\tint\t\tSpare\(void\);$',
		 r'^18\|\tvoid\tRoll\(void\) override;$', r'^32 lines$', r'^19\|int CDrum::Spare\(void\)$', r'^29\|void CDrum::Roll\(void\)$', r'^72 lines$',
		 r'^\d+\|    <ClCompile Include="Barrel\.cpp" />$', r'^\d+\|    <ClInclude Include="Barrel\.h" />$'],
		[r'CDrum::Fill']),
	('extract-interface', dict(MakeMoveMembersCopy(), **{
		#Extract Interface: the functions chosen are declared pure virtual in the new class, CDrum keeps its own and
		#derives from it.  A field can't go in one.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:extractinterface:CFillable|Fill(int);miLevel,edit:extractinterface:CFillable|Fill(int);Roll(),printdisklines:Fillable.h,printdisklines:Drum.h,printdisklines:Drum.cpp'}),
		[r"^Extract Class: an interface has only functions, and miLevel isn't one$", r'^Status: Extracted the interface CFillable from CDrum: 2 members, in Fillable\.h\.$',
		 r'^6\|class CFillable$', r'^8\|public:$', r'^9\|\tvirtual void\tFill\(int iAmount\) =0;$', r'^10\|\tvirtual void\tRoll\(void\) =0;$', r'^11\|\};$', r'^14 lines$',
		 r'^4\|#include "Fillable\.h"$', r'^7\|class CDrum : public CVessel, public CFillable$', r'^13\|\tint\t\tmiLevel;$', r'^20\|\tvoid\tFill\(int iAmount\) override;$', r'^22\|\tvoid\tRoll\(void\) override;$', r'^36 lines$',
		 r'^19\|void CDrum::Fill\(int iAmount\)$', r'^82 lines$'],
		[r'^Conflict']),
	('extract-class', dict(MakeMoveMembersCopy(), **{
		#Extract Class: the members become CGauge, all public, and CDrum has a field of it where the first of them
		#was; what CDrum still does with them goes through the field (mcGauge.miLevel, mcGauge.Fill(1)).  CGauge has
		#an Init of its own, empty, and CDrum's Init calls it after its base class's.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8',
		'DEBUG': 'edit:renamesearch:---s,edit:extractclass:CGauge|miSecret|miLevel,edit:extractclass:CGauge|mcGauge|miLevel;GetLevel();Fill(int),'
		         'printdisklines:Gauge.h,printdisklines:Gauge.cpp,printdisklines:Drum.h,printdisklines:Drum.cpp'}),
		[r'^Extract Class: the class has a miSecret already$', r'^Status: Extracted the class CGauge from CDrum: 3 members, in Gauge\.h and Gauge\.cpp\.$',
		 r'^6\|class CGauge$', r'^8\|public:$', r'^9\|\tint\t\tmiLevel;$', r'^10\|$', r'^11\|\tvoid\tInit\(void\);$', r'^12\|\t//How full it is\.$', r'^13\|\tint\t\tGetLevel\(void\) \{ return miLevel; \}$',
		 r'^14\|\tvoid\tFill\(int iAmount\);$', r'^15\|\};$',
		 r'^9\|void CGauge::Init\(void\)$', r'^10\|\{$', r'^11\|\}$', r'^18\|void CGauge::Fill\(int iAmount\)$', r'^20\|\tmiLevel \+= iAmount;$',
		 r'^4\|#include "Gauge\.h"$', r'^7\|class CDrum : public CVessel$', r'^12\|protected:$', r'^13\|\tCGauge\tmcGauge;$', r'^14\|\tint\t\tmiRate, miStep;$', r'^17\|\tvoid\tInit\(int iVolume\);$', r'^18\|\tint\t\tSpare\(void\);$',
		 r'^10\|\tCVessel::Init\(iVolume\);$', r'^11\|\tmcGauge\.Init\(\);$', r'^12\|\tmcGauge\.miLevel = 0;$', r'^32\|\tmcGauge\.Fill\(1\);$', r'^73 lines$'],
		[r'^Conflict']),
	('extract-superclass-dialog', dict(MakeMoveMembersCopy(), **{
		#The dialog: the new class's name typed, and the members in a table, the one at the caret ticked; Refactor
		#extracts it.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h', 'CARET': '20:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'type:CBarrel', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'extractsuperclass,answered:1,printstatus'}),
		[r'^Dialog \[Extract Superclass from CDrum\] answered OK$', r'^Status: Extracted the superclass CBarrel from CDrum: 1 member, in Barrel\.h and Barrel\.cpp\.$'],
		[]),
	('extract-superclass-template', dict(MakeMoveMembersCopy(), **{
		#From a class template, where what moves names the template's parameter: the new class is a template of the
		#same parameters, the class derives from it with them, and the function defined outside the class goes with
		#its body in the new class.  The dialog (Cancel) is Extract Superclass's.
		'WAIT_INDEX': '1', 'OPEN': 'Crate.h', 'CARET': '19:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:renamesearch:---s,extractsuperclass,answered:1,edit:extractsuperclass:CSized|Size(),printdisklines:Sized.h,printdisklines:Crate.h'}),
		[r'^Dialog \[Extract Superclass from CCrate\] answered Cancel$', r'^Status: Extracted the superclass CSized from CCrate: 1 member, in Sized\.h\.$',
		 r'^5\|template<class M>$', r'^6\|class CSized$', r'^8\|public:$', r'^9\|\tint\t\tSize\(void\) \{ return sizeof\(M\); \}$', r'^13 lines$',
		 r'^3\|#include "Sized\.h"$', r'^16\|template<class M>$', r'^17\|class CCrate : public CBox<M>, public CSized<M>$', r'^23 lines$'],
		[r'CCrate<M>::Size']),
	('encapsulate-field', dict(MakeEncapsulateFieldCopy(), **{
		#Encapsulate Field: every place the field is named, and how (the compiler says).  The class declares GetValue
		#(const) and SetValue at the end of its public part and its source file defines them at its bottom, a line
		#each; the field goes to a protected part made at the class's start, and
		#outside the class a read is the getter, an assignment the setter (what is assigned may read the field too),
		#and a += or a ++ the setter of the getter's value changed.  The class's own functions still name the field.
		'WAIT_INDEX': '1', 'OPEN': 'Dial.h', 'CARET': '14:8',
		'DEBUG': 'edit:renamesearch:---s,edit:encapsulable,edit:encapsulate:gsp,printdisklines:Dial.h,printdisklines:Dial.cpp'}),
		[r'^Encapsulate Field: CDial::miValue, \[int\], public, GetValue and SetValue\(iValue\)$', r'^  Dial\.h line 20: read, in the class$', r'^  Dial\.cpp line 10: assigned, in the class$',
		 r'^  Dial\.cpp line 28: assigned$', r'^  Dial\.cpp line 28: read$', r'^  Dial\.cpp line 29: changed with an operator$', r'^  Dial\.cpp line 30: stepped$',
		 r'^Status: Encapsulated CDial::miValue: GetValue and SetValue, 6 places changed\.$',
		 r'^11\|class CDial$', r'^12\|\{$', r'^13\|protected:$', r'^14\|\tint\t\tmiValue;$', r'^15\|$', r'^16\|public:$', r'^17\|\tint\t\tmiStep;$', r'^22\|\tint\t\tTwice\(void\) \{ return miValue \* 2; \}$',
		 r'^23\|\tint\t\tGetValue\(void\) const;$', r'^24\|\tvoid\tSetValue\(int iValue\);$', r'^25\|\};$',
		 r'^10\|\tmiValue = 0;$', r'^28\|\tcOther\.SetValue\(pcDial->GetValue\(\) \+ 1\);$', r'^29\|\tpcDial->SetValue\(pcDial->GetValue\(\) \+ 2\);$', r'^30\|\tpcDial->SetValue\(pcDial->GetValue\(\) \+ 1\);$',
		 r'^31\|\tiRead = cOther\.GetValue\(\) \* pcDial->GetValue\(\);$', r'^32\|\tpiStep = &pcDial->miStep;$',
		 r'^38\|$', r'^39\|$', r'^40\|int CDial::GetValue\(void\) const \{ return miValue; \}$', r'^41\|void CDial::SetValue\(int iValue\) \{ miValue = iValue; \}$', r'^41 lines$'],
		[r'^Conflict']),
	('encapsulate-field-object', dict(MakeEncapsulateFieldCopy(), **{
		#A field that is an object has a getter that gives a pointer to it, and no setter: a member named through it
		#goes through the getter, and its address is the getter.  And a field of a class that has a protected part
		#goes to the end of that part; with i its class's own functions use the getter and setter too.
		'WAIT_INDEX': '1', 'OPEN': 'Dial.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:30:8,edit:encapsulate:gspi,edit:caret:16:9,edit:encapsulable,edit:encapsulate:gs,edit:encapsulate:gp,printdisklines:Dial.h,printdisklines:Dial.cpp'}),
		[r'^Status: Encapsulated CKnob::miGrip: GetGrip and SetGrip, 1 place changed\.$',
		 r'^Encapsulate Field: CDial::msMark, \[SMark\], public, GetMark and SetMark\(sMark\)$', r'^  Dial\.cpp line 12: a member of it named, in the class$', r'^  Dial\.cpp line 33: a member of it named$',
		 r'^  Dial\.cpp line 34: its address taken$', r'^Encapsulate Field: A field that is an object has no setter: its getter gives a pointer to it\.$',
		 r'^Status: Encapsulated CDial::msMark: GetMark, 2 places changed\.$',
		 r'^13\|protected:$', r'^14\|\tSMark\tmsMark;$', r'^16\|public:$', r'^23\|\tSMark\*\tGetMark\(void\);$',
		 r'^\d+\|class CKnob$', r'^\d+\|protected:$', r'^\d+\|\tint\t\tmiTurn;$', r'^\d+\|\tint\t\tmiGrip;$', r'^\d+\|$', r'^\d+\|public:$', r'^\d+\|\tvoid\tInit\(void\) \{ miTurn = 0; \}$',
		 r'^36\|\tint\t\tGetGrip\(void\) const;$', r'^37\|\tvoid\tSetGrip\(int iGrip\);$',
		 r'^12\|\tmsMark\.iAt = 0;$', r'^33\|\tpcDial->GetMark\(\)->iAt = iRead;$', r'^34\|\tpsMark = cOther\.GetMark\(\);$', r'^35\|\tpcKnob->SetGrip\(\*piStep \+ psMark->iAt\);$',
		 r'^40\|int CKnob::GetGrip\(void\) const \{ return miGrip; \}$', r'^41\|void CKnob::SetGrip\(int iGrip\) \{ miGrip = iGrip; \}$', r'^42\|SMark\* CDial::GetMark\(void\) \{ return &msMark; \}$'],
		[r'^Conflict']),
	('encapsulate-field-refused', dict(MakeEncapsulateFieldCopy(), **{
		#A place that uses the field in a way no getter or setter stands for (its address is taken) stays, and is
		#in the way if the field is to be protected: nothing is done.  A field declared with another is refused, and
		#a function is no field.  The dialog is Encapsulate's.
		'WAIT_INDEX': '1', 'OPEN': 'Dial.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:caret:15:8,edit:encapsulate:gsp,edit:caret:17:8,edit:encapsulable,edit:caret:20:8,edit:encapsulable,edit:caret:14:8,encapsulatefield,answered:1,printstatus,printdisklines:Dial.h'}),
		[r"^Conflict: miStep is used in Dial\.cpp at line 32 in a way a getter or a setter can't stand for: it stays, and won't compile once the field is protected\.$",
		 r"^Encapsulate Field: CDial::miRange can't be: it is declared with others in one declaration$", r'^Encapsulate Field: no field$', r'^Dial\.h|^Dialog \[Encapsulate CDial::miValue\] answered Cancel$',
		 r'^15\|\tint\t\tmiStep;$', r'^36 lines$'],
		[r'^Status: Encapsulated']),
	('includes-optimise', dict(MakeIncludesCopy(), **{
		#Optimise #includes on a header: the #include nothing it names comes from goes, the one only a class named as
		#a pointer comes from becomes the class's forward declaration, the one whose macro it uses stays, and the files
		#that got names through those that went are given their own #includes.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:includes,edit:optimiseincludes,printdisklines:Tank.h,printdisklines:Tank.cpp,printdisklines:Main.cpp'}),
		[r'^Optimise #includes: Tank\.h$', r'^  "Gauge\.h" \(line 3\): a forward declaration: class CGauge;$', r'^  "Valve\.h" \(line 4\): kept, CValve comes from it$',
		 r'^  "Spare\.h" \(line 5\): removed, nothing named here comes from it$', r'^  "Units\.h" \(line 6\): kept, its macro MAX_UNITS is used$',
		 r'^  Main\.cpp is given #include "Gauge\.h"$', r'^  Main\.cpp is given #include "Spare\.h"$', r'^  Tank\.cpp is given #include "Gauge\.h"$',
		 r'^Status: Optimised the #includes of Tank\.h: 1 removed, 1 made a forward declaration; 2 files that got names through it given their own #includes\.$',
		 r'^2\|#define __TANK_H__$', r'^3\|#include "Valve\.h"$', r'^4\|#include "Units\.h"$', r'^5\|$', r'^6\|$', r'^7\|class CGauge;$', r'^8\|$', r'^9\|$', r'^10\|class CTank$',
		 r'^1\|#include "Tank\.h"$', r'^2\|#include "Gauge\.h"$', r'^3\|#include "Holder\.h"$', r'^4\|#include "Gauge\.h"$', r'^5\|#include "Spare\.h"$'],
		[r'^\d+\|#include "Spare\.h"\n(.*\n)*Disk lines Tank\.cpp']),
	('includes-added', dict(MakeIncludesCopy(), **{
		#A header that #includes nothing and compiles where its source has what it needs first: the header of the
		#class it has a member of is given an #include, and the class it only points to a forward declaration.  Its
		#source's #includes, all of its own directory, stay in the order they are, its own header where it is.  A
		#template's header isn't given the header of
		#what its instances are made with.  A source that names a class whose header it hasn't is given the #include.
		'WAIT_INDEX': '1', 'OPEN': 'Drum.h',
		'DEBUG': 'edit:renamesearch:---s,edit:includes,edit:optimiseincludes,edit:open:Drum.cpp,edit:optimiseincludes,edit:open:Holder.h,edit:optimiseincludes,edit:open:Flow.cpp,edit:optimiseincludes,printdisklines:Drum.h,printdisklines:Drum.cpp,printdisklines:Flow.cpp'}),
		[r'^Optimise #includes: Drum\.h$', r'^  added: #include "Pipe\.h", CPipe comes from it$', r'^  added: class CGauge;$',
		 r'^Status: Optimised the #includes of Drum\.h: 1 added, 1 forward declaration added\.$', r'^Status: The #includes of Drum\.cpp are as they should be\.$',
		 r'^Status: The #includes of Holder\.h are as they should be\.$', r'^Status: Optimised the #includes of Flow\.cpp: 1 added\.$', r'^  added: #include "Valve\.h", CValve comes from it$',
		 r'^2\|#define __DRUM_H__$', r'^3\|#include "Pipe\.h"$', r'^6\|class CGauge;$', r'^9\|class CDrum$',
		 r'^1\|#include "Pipe\.h"$', r'^2\|#include "Gauge\.h"$', r'^3\|#include "Drum\.h"$', r'^2\|#include "Valve\.h"$'],
		[]),
	('includes-needed', dict(MakeIncludesCopy(), **{
		#An #include nothing the file names comes from, that the header after it needs: the file is checked as it
		#would be, and it stays.  A <> #include, one inside an
		#if and one whose file isn't found are left as they are.
		'WAIT_INDEX': '1', 'OPEN': 'Sink.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:includes,edit:optimiseincludes,edit:open:Drum.cpp,edit:optimiseincludes,edit:includes,edit:open:Wide.cpp,edit:includes,edit:optimiseincludes,printdisklines:Sink.cpp,printdisklines:Drum.cpp'}),
		[r'^Optimise #includes: Sink\.cpp$', r'^  "Units\.h" \(line 1\): removed, nothing named here comes from it$', r'^  "Spare\.h" \(line 3\): removed, nothing named here comes from it$',
		 r"^Status: Optimised the #includes of Sink\.cpp: 1 removed; 1 kept that nothing named here comes from: the file doesn't compile without it\.$",
		 r"^  \"Units\.h\" \(line 1\): needed, the file doesn't compile without it$",
		 r'^Status: The #includes of Drum\.cpp are as they should be\.$',
		 r'^  "Drum\.h" \(line 3\): kept, the file\'s own header$',
		 r'^  <string\.h> \(line 1\): left, a <> #include$', r'^  "Spare\.h" \(line 4\): left, inside an #if$', r"^  \"Missing\.h\" \(line 6\): left, its file isn't found$",
		 r'^Status: The #includes of Wide\.cpp are as they should be\.$',
		 r'^1\|#include "Units\.h"$', r'^2\|#include "Order\.h"$', r'^3\|$', r'^1\|#include "Pipe\.h"$', r'^3\|#include "Drum\.h"$'],
		[]),
	('includes-order', dict(MakeIncludesCopy(), **{
		#Optimise #includes puts them in order: the <> one first, then each library's (the directory its name is
		#written with), the library another's headers reach before it, then the file's own directory's; one added
		#goes after the last of its library.  A file that doesn't compile with them in order (a header needs what
		#the one before it declares) is left as it is.
		'WAIT_INDEX': '1', 'OPEN': 'Build.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:optimiseincludes,edit:open:Odd.cpp,edit:optimiseincludes,printdisklines:Build.cpp,printdisklines:Odd.cpp'}),
		[r'^Status: Optimised the #includes of Build\.cpp: 1 added, put in order\.$', r'^  added: #include "Low/Washer\.h", SWasher comes from it$',
		 r"^Status: The #includes of Odd\.cpp are as they should be; left in the order they are: the file doesn't compile with them in order \(a header needs another before it\); 1 kept ",
		 r'^1\|#include <string\.h>$', r'^2\|#include "Low/Nut\.h"$', r'^3\|#include "Low/Washer\.h"$', r'^4\|#include "High/Frame\.h"$', r'^5\|#include "Rig\.h"$', r'^6\|$',
		 r'^1\|#include "Units\.h"$', r'^2\|#include "High/Span\.h"$'],
		[]),
	('includes-ask', dict(MakeIncludesCopy(), **{
		#The command asks before it changes other files than the one in the editor: what each is given is shown as
		#Problems Detected.  Cancel changes nothing; Continue does it all.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel|OK',
		'DEBUG': 'edit:renamesearch:---s,command:Refactor.OptimiseIncludes,answered:1,printstatus,printdisklines:Tank.h,command:Refactor.OptimiseIncludes,answered:2,printstatus,printdisklines:Tank.h'}),
		[r'^Dialog \[Problems Detected\] answered Cancel$', r'^Status: The #includes of Tank\.h are left as they were\.$', r'^3\|#include "Gauge\.h"$',
		 r'^Dialog \[Problems Detected\] answered OK$',
		 r'^Status: Optimised the #includes of Tank\.h: 1 removed, 1 made a forward declaration; 2 files that got names through it given their own #includes\.$'],
		[]),
	('macro-found', dict(MakeMacrosCopy(), **{
		#Convert Macro: the macro at the caret, its #define and every place its name is written, and what it would
		#become or why it can't: one an #if or an #ifdef names, one whose body has no brackets round it, one that
		#changes its argument, one that makes a string of it, one whose parameter's type is declared after it.  One
		#whose argument is a call the body has twice is shown as a conflict; one used with ints and with floats would
		#be a function template.
		'WAIT_INDEX': '1', 'OPEN': 'Macros.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:9:10,edit:macro,edit:caret:10:10,edit:macro,edit:caret:18:10,edit:macro,edit:caret:19:10,edit:macro,edit:caret:20:10,edit:macro,edit:caret:21:10,edit:macro,edit:caret:17:10,edit:macro,edit:caret:16:10,edit:macro'}),
		[r'^Macro: LIMIT \[4\], Macros\.h line 9$', r'^  Flags\.cpp line 7: on an #if$', r'^  Flags\.cpp line 8: used$', r"^Convert Macro: LIMIT can't be: an #if names it \(Flags\.cpp line 7\)$", r'^Enum of: LIMIT$',
		 r"^Convert Macro: DEBUG_UNITS can't be: an #if names it \(Flags\.cpp line 4\)$",
		 r'^Macro: ADD\(a, b\) \[a \+ b\], Macros\.h line 18$', r'^  Use\.cpp line 33: used with \[iCount\], \[1\]$',
		 r"^Convert Macro: ADD can't be: what it makes isn't one value or one statement where it is used: its body needs brackets round it \(Use\.cpp line 33\)$",
		 r"^Convert Macro: BUMP can't be: it changes its argument: a function would change its own copy$",
		 r"^Convert Macro: NAME_OF can't be: its body has # or ##: only a macro can make a string or a name of its arguments$",
		 r"^Convert Macro: RESET can't be: the code doesn't compile with it a function: its body or its parameters' types name something that isn't declared where it is #defined, or a use gives it what a function can't take$",
		 r'^Macro: SQUARE\(x\) \[\(\(x\) \* \(x\)\)\], Macros\.h line 17$', r'^Converted: inline int SQUARE\(int x\) \{ return \(\(x\) \* \(x\)\); \}$',
		 r'^Conflict: Next\(psUnit\), in Use\.cpp at line 32, is worked out 2 times by the macro and would be once by a function\.$',
		 r'^  Use\.cpp line 29: used with \[iCount\], \[MAX_UNITS\]$', r'^  Use\.cpp line 30: used with \[fRate\], \[2\.0f\]$', r'^Converted: template<class T>$',
		 r'^inline T BIGGER\(T a, T b\) \{ return \(\(a\) > \(b\) \? \(a\) : \(b\)\); \}$'],
		[r'^Status']),
	('macro-convert', dict(MakeMacrosCopy(), **{
		#Convert Macro done: #defines of values become constants of the types the compiler gives them (a string a
		#char array), a run of #defines of whole numbers one enum, and macros with parameters inline functions with
		#the types their uses give them (a function template where those differ; void for one that is statements;
		#one of several lines on one).
		'WAIT_INDEX': '1', 'OPEN': 'Macros.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:38:10,edit:macroconvert,edit:caret:23:10,edit:macroconvert,edit:caret:22:10,edit:macroconvert,edit:caret:17:10,edit:macroconvert,edit:caret:16:10,edit:macroconvert,edit:caret:13:10,edit:macroenum:EKind,edit:caret:8:10,edit:macroconvert,edit:caret:7:10,edit:macroconvert,edit:caret:6:10,edit:macroconvert,edit:caret:5:10,edit:macroconvert,printdisklines:Macros.h'}),
		[r'^Status: Converted the macro CLEAR to a function\.$', r'^Status: Converted the macro LONGER to a function\.$', r'^Status: Converted the macro TWICE to a function\.$',
		 r'^Conflict: Next\(psUnit\), in Use\.cpp at line 32, is worked out 2 times by the macro and would be once by a function\.$', r'^Status: Converted the macro SQUARE to a function\.$',
		 r'^Status: Converted the macro BIGGER to a function template\.$', r'^Status: Converted 3 #defines to the enum EKind\.$', r'^Status: Converted the macro RATE to a constant\.$',
		 r'^Status: Converted the macro TITLE to a constant\.$', r'^Status: Converted the macro SCALE to a constant\.$', r'^Status: Converted the macro MAX_UNITS to a constant\.$',
		 r'^5\|const int MAX_UNITS = 8;  //The most there are\.$', r'^6\|const int SCALE = \(MAX_UNITS \* 2\);$', r'^7\|const char TITLE\[\] = "Units";$', r'^8\|const float RATE = 1\.5f;$',
		 r'^9\|#define LIMIT\t\t4$', r'^12\|enum EKind$', r'^13\|\{$', r'^14\|\tKIND_NONE,\t\t//Nothing\.$', r'^15\|\tKIND_SOLID,$', r'^16\|\tKIND_LIQUID,$', r'^17\|\};$',
		 r'^19\|template<class T>$', r'^20\|inline T BIGGER\(T a, T b\) \{ return \(\(a\) > \(b\) \? \(a\) : \(b\)\); \}$', r'^21\|inline int SQUARE\(int x\) \{ return \(\(x\) \* \(x\)\); \}$',
		 r'^22\|#define ADD\(a, b\)\t\ta \+ b$', r'^26\|inline int TWICE\(int x\) \{ return \(\(x\) \+ \(x\)\); \}$', r'^27\|inline int LONGER\(int a, int b\) \{ return \(\(a\) \+ \(b\)\); \}$',
		 r'^\d+\|inline void CLEAR\(SUnit\* p\) \{ \(p\)->miCount = 0; \(p\)->miKind = KIND_NONE; \}$'],
		[r"can't be"]),
	('macro-inline', dict(MakeMacrosCopy(), **{
		#Inline Macro: every use replaced by what the preprocessor makes of it and the #define taken out (a use in
		#another's argument with it), or only the use at the caret (a string made of its argument; a macro of several
		#lines; arguments put in the body).  A name that is no macro is none; one an #if names can't go from all its
		#uses, and at its #define there is no use to inline.
		'WAIT_INDEX': '1', 'OPEN': 'Use.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:39:24,edit:macro,edit:macroinline:all,edit:caret:39:16,edit:macroinline:this,edit:caret:35:13,edit:macroinline:this,edit:caret:40:16,edit:macroinline:all,edit:caret:30:10,edit:macroinline:this,edit:caret:19:6,edit:macro,edit:open:Macros.h,edit:caret:9:10,edit:macroinline:all,edit:macroinline:this,printdisklines:Use.cpp,printdisklines:Macros.h'}),
		[r'^Macro: TWICE\(x\) \[\(\(x\) \+ \(x\)\)\], Macros\.h line 22$', r'^  Use\.cpp line 39: used with \[TWICE\(iCount\)\], at the caret$', r'^  Use\.cpp line 39: used with \[iCount\]$',
		 r'^Status: Inlined the macro TWICE at 2 uses and took its #define out\.$', r'^Status: Inlined the macro SCALE here\.$', r'^Status: Inlined the macro NAME_OF here\.$',
		 r'^Status: Inlined the macro LONGER at 1 use and took its #define out\.$', r'^Status: Inlined the macro BIGGER here\.$', r'^Macro: none$',
		 r'^Inline Macro: An #if names it: only a macro can be there \(Flags\.cpp line 7\)\.$', r"^Inline Macro: The caret isn't on a use of it\.$",
		 r'^30\|\tfBig = \(\(fRate\) > \(2\.0f\) \? \(fRate\) : \(2\.0f\)\);$', r'^35\|\tszTitle = "iBig";$',
		 r'^39\|\taiUnits\[0\] = \(MAX_UNITS \* 2\) \+ \(\(\(\(iCount\) \+ \(iCount\)\)\) \+ \(\(\(iCount\) \+ \(iCount\)\)\)\);$', r'^40\|\taiUnits\[1\] = \(\(iBig\) \+ \(2\)\);$',
		 r'^21\|#define RESET\(p\)', r'^22\|$', r'^23\|$', r'^24\|struct SUnit$'],
		[r'#define TWICE', r'#define LONGER']),
	('macro-dialogs', dict(MakeMacrosCopy(), **{
		#Convert Macro's dialog, and Inline...'s for a macro (the key that inlines a variable or a function).
		'WAIT_INDEX': '1', 'OPEN': 'Macros.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:caret:5:10,convertmacro,answered:1,edit:caret:22:10,inlinevariable,answered:2,edit:caret:19:10,convertmacro,printstatus,printdisklines:Macros.h'}),
		[r'^Dialog \[Convert Macro MAX_UNITS\] answered Cancel$', r'^Dialog \[Inline Macro TWICE\] answered Cancel$',
		 r"^Status: Can't convert the macro BUMP: it changes its argument: a function would change its own copy\.$", r'^5\|#define MAX_UNITS\t8\t//The most there are\.$'],
		[r'^Status: Converted', r'^Status: Inlined']),
	('template-interface', dict(MakeRenameTemplateCopy(), **{
		#Write Template's Interface: CHolder<M> calls mp->Init() and reads mp->miCount, which the classes it is used
		#with declare.  Written as a comment over the template; as a class it is refused for the data member.
		'WAIT_INDEX': '1', 'OPEN': 'Holder.h', 'CARET': '10:1',
		'DEBUG': 'edit:renamesearch:---s,edit:templateinterface:class:CHolderItem,edit:templateinterface:comment,printdisklines:Holder.h'}),
		[r'^Template Interface: CHolder, 2 members$', r'^  M: void\tInit\(void\)$', r'^  M: int\tmiCount$',
		 r'^Status: Write Template.s Interface: A class can ask for functions only: the template needs a data member\.$',
		 r'^Status: Wrote what CHolder needs of its parameters as a comment: 2 members\.$',
		 r'^4\|$', r'^5\|//M must have:$', r'^6\|//\tvoid\tInit\(void\);$', r'^7\|//\tint\t\tmiCount;$', r'^8\|template<class M>$', r'^19 lines$'],
		[]),
	('template-interface-derive', dict(MakeTemplateInterfaceCopy(), **{
		#As an abstract class, with the classes the template is used with made to derive from it: CDiesel gets
		#": public CRunnable", its three functions override, and Diesel.h an #include of Runner.h.
		'WAIT_INDEX': '1', 'OPEN': 'Runner.h', 'CARET': '12:8',
		'DEBUG': 'edit:renamesearch:---s,edit:templateinterface:derive:CRunnable,printdisklines:Diesel.h,printdisklines:Runner.h'}),
		[r'^Status: Wrote what CRunner needs of M as the class CRunnable: 3 members; CDiesel derives from it\.$',
		 r'^3\|#include "Runner\.h"$', r'^6\|class CDiesel : public CRunnable$', r'^11\|\tvoid\t\t\tStart\(int iSpeed\) override \{ miRuns = iSpeed; \}$',
		 r'^12\|\tvirtual void\tStop\(void\) override \{ miRuns = 0; \}$', r'^13\|\tint\t\t\t\tCount\(void\) const override \{ return miRuns; \}$', r'^17 lines$',
		 r'^5\|class CRunnable$', r'^8\|\tvirtual void\tStart\(int iSpeed\) =0;$'],
		[]),
	('template-interface-dialog', dict(MakeTemplateInterfaceCopy(), **{
		#The command's dialog shows the members and asks how to write them; as it opens, a comment.
		'WAIT_INDEX': '1', 'OPEN': 'Runner.h', 'CARET': '12:8', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'command:Refactor.TemplateInterface,answered:1,printstatus'}),
		[r'^Dialog \[What CRunner Needs of Its Parameters\] answered OK$', r'^Status line: Wrote what CRunner needs of its parameters as a comment: 3 members\.$'],
		[]),
	('template-interface-class', dict(MakeTemplateInterfaceCopy(), **{
		#As an abstract class: what CRunner<M> calls of M, in the class and in its function defined outside it (which
		#names the parameter T), each as CDiesel declares it (without its virtual and its body), before the comment
		#over the template.  A function template nothing uses has no declarations to go by: only a comment.
		'WAIT_INDEX': '1', 'OPEN': 'Runner.h', 'CARET': '12:8',
		'DEBUG': 'edit:renamesearch:---s,edit:templateinterface:class:CDiesel,edit:templateinterface:class:CRunnable,edit:caret:36:3,edit:templateinterface:class:CPokeable,'
				 'edit:templateinterface:comment,printdisklines:Runner.h'}),
		[r'^Template Interface: CRunner, 3 members$', r'^  M: void\tStart\(int iSpeed\)$', r'^  M: void\tStop\(void\)$', r'^  M: int\tCount\(void\) const$',
		 r'^Status: Write Template.s Interface: Something has that name already\.$',
		 r'^Status: Wrote what CRunner needs of M as the class CRunnable: 3 members\.$',
		 r'^Template Interface: Poke, 1 member$', r'^  M: Wake\(\?, \?\)$',
		 r'^Status: Write Template.s Interface: A class needs each member.s declaration: no class the template is used with says how one is declared\.$',
		 r'^Status: Wrote what Poke needs of its parameters as a comment: 1 member\.$',
		 r'^5\|class CRunnable$', r'^7\|public:$', r'^8\|\tvirtual void\tStart\(int iSpeed\) =0;$', r'^9\|\tvirtual void\tStop\(void\) =0;$', r'^10\|\tvirtual int\t\tCount\(void\) const =0;$',
		 r'^11\|\};$', r'^12\|$', r'^13\|$', r'^14\|//Runs what it is given\.$', r'^15\|template<class M>$',
		 r'^34\|//M must have:$', r'^35\|//\tWake\(\?, \?\);$', r'^36\|template<class M>$', r'^43 lines$'],
		[]),
	('enum-to-class-no-casts', dict(MakeEnumsCopy(), **{
		#With the dialog's tick off ("Write casts where its values are used as integers") an enum whose values are
		#used as integers is refused, with the places; one whose aren't is converted.  The dialog's tick is the
		#first thing in it: space takes it off.
		'WAIT_INDEX': '1', 'OPEN': 'Modes.h', 'SETTINGS': MakeSettingsFile(), 'DIALOG_KEYS': 'space', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:13:7,edit:enumtoclass:nocasts,edit:caret:25:4,edit:enumtoclass:nocasts,edit:caret:13:7,command:Refactor.EnumToEnumClass,answered:1,printstatus'}),
		[r"^Status: Can't make ELevel an enum class: its values are used as integers, and casts aren't to be written: Use\.cpp line 16 and 4 other places\.$",
		 r'^Status: Made EState an enum class: 3 uses of its enumerators given its name, in 3 files\.$',
		 r'^Focus: CCheckBox$', r'^Dialog \[Convert ELevel to Enum Class\] answered OK$',
		 r"^Status line: Can't make ELevel an enum class: its values are used as integers, and casts aren't to be written: Use\.cpp line 16 and 4 other places\.$"],
		[r'^Dialog \[Problems Detected\]']),
	('enum-to-class-casts-shown', dict(MakeEnumsCopy(), **{
		#The command shows where casts are written before it changes anything: Cancel there changes nothing, Continue
		#does it.
		'WAIT_INDEX': '1', 'OPEN': 'Modes.h', 'CARET': '13:7', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'OK|Cancel|OK|OK',
		'DEBUG': 'edit:renamesearch:---s,command:Refactor.EnumToEnumClass,answered:2,printstatus,command:Refactor.EnumToEnumClass,answered:4,printstatus'}),
		[r'^Dialog \[Problems Detected\] answered Cancel$', r'^Status line: Convert Enum to Enum Class: nothing was changed\.$',
		 r'^Dialog \[Problems Detected\] answered OK$',
		 r'^Status line: Made ELevel an enum class: 5 uses of its enumerators given its name, in 3 files; casts written at 5 places\.$'],
		[]),
	('enum-to-class', dict(MakeEnumsCopy(), **{
		#Convert Enum to Enum Class: EMode's enum becomes enum class and its enumerators get its name wherever they
		#are named without it (not in its own list, nor where it is written already); from an enumerator, CMotor's
		#EState, whose enumerators are named in the class and with the class's name.  ELevel's value is added to, used
		#as an index and as a condition, and given to an int: each gets its cast.  EFlag's are or-ed and and-ed, and
		#|= is written out.  EStep's is stepped with ++: refused.
		'WAIT_INDEX': '1', 'OPEN': 'Modes.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:5:7,edit:enumtoclass,edit:caret:25:4,edit:enumtoclass,edit:caret:13:7,edit:enumtoclass,edit:caret:39:7,edit:enumtoclass,edit:caret:20:8,edit:convertableenum,'
				 'edit:open:Use.cpp,edit:caret:41:7,edit:enumtoclass,printdisklines:Modes.h,printdisklines:Modes.cpp,printdisklines:Use.cpp'}),
		[r'^Convert Enum: EMode, 3 enumerators, 1 declaration, 4 uses in 3 files, casts at 1 place$', r'^Status: Made EMode an enum class: 4 uses of its enumerators given its name, in 3 files; casts written at 1 place\.$',
		 r'^Convert Enum: EState, 2 enumerators, 1 declaration, 3 uses in 2 files$', r'^Status: Made EState an enum class: 3 uses of its enumerators given its name, in 3 files\.$',
		 r'^Convert Enum: ELevel, 2 enumerators, 1 declaration, 5 uses in 2 files, casts at 5 places$',
		 r'^Status: Made ELevel an enum class: 5 uses of its enumerators given its name, in 3 files; casts written at 5 places\.$',
		 r'^Convert Enum: EFlag, 2 enumerators, 1 declaration, 4 uses in 1 file, casts at 3 places$',
		 r'^Status: Made EFlag an enum class: 4 uses of its enumerators given its name, in 2 files; casts written at 3 places\.$',
		 r'^Convert Enum: no enum at the caret$',
		 r"^Convert Enum: EStep can't be: its value is stepped with \+\+ or --, which an enum class's can't be \(Use\.cpp line 57\)$",
		 r"^Status: Can't make EStep an enum class: its value is stepped with \+\+ or --, which an enum class's can't be \(Use\.cpp line 57\)\.$",
		 r'^5\|enum class EMode$', r'^9\|\tEM_Fast = \(int\)EM_Slow \* 2,$', r'^13\|enum class ELevel$', r'^23\|\tenum class EState$', r'^34\|\tbool\tIsFast\(void\) \{ return meMode == EMode::EM_Fast; \}$',
		 r'^39\|enum class EFlag$', r'^46 lines$',
		 r'^10\|\tmeMode = EMode::EM_Off;$', r'^11\|\tmeState = EState::ES_Idle;$', r'^12\|\tmeLevel = ELevel::EL_Low;$', r'^24\|\tcase EMode::EM_Slow:$', r'^26\|\tcase EMode::EM_Fast:$',
		 r'^29\|\t\treturn \(meState == EState::ES_Running\) \? 3 : 0;$', r'^31 lines$',
		 r'^13\|\tpcMotor->meMode = EMode::EM_Fast;$', r'^14\|\tpcMotor->meState = CMotor::EState::ES_Running;$', r'^15\|\tpcMotor->meLevel = ELevel::EL_High;$',
		 r'^16\|\tiNext = \(int\)pcMotor->meLevel \+ 1;$', r'^17\|\taiCounts\[\(int\)pcMotor->meLevel\] = iNext;$', r'^18\|\tif \(\(int\)pcMotor->meLevel\)$', r'^20\|\t\tiNext = \(int\)ELevel::EL_High;$',
		 r'^22\|\treturn \(pcMotor->meLevel == ELevel::EL_High\) \? iNext : aiCounts\[\(int\)ELevel::EL_Low\];$',
		 r'^34\|\teFlags = EFlag::EF_Read;$', r'^35\|\teFlags = \(EFlag\)\(\(int\)eFlags \| \(int\)EFlag::EF_Write\);$', r'^36\|\teFlags = \(EFlag\)\(\(int\)eFlags \| \(int\)EFlag::EF_Read\);$',
		 r'^37\|\treturn \(\(int\)eFlags & \(int\)EFlag::EF_Write\) \? 1 : 0;$', r'^41\|enum EStep$', r'^57\|\teStep\+\+;$'],
		[]),
	('pointer-parameter', dict(MakePointersCopy(), **{
		#Convert Pointer to Reference on a parameter: its * a & in every declaration and definition of its function
		#and of the function that overrides it, p->x made p.x, (*p).x made p.x, *p made p, p given on made &p, and
		#every call's argument: &c made c, a pointer made *p.  A call of the function itself, and of the function it
		#overrides, with the parameter stays as it is.  And back, a reference made a pointer.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:30:25,edit:pointertoreference,edit:caret:94:24,edit:pointertoreference,edit:caret:54:32,edit:referencetopointer,printdisklines:Vat.h,printdisklines:Vat.cpp'}),
		[r'^Status: Converted the parameter psLevel of CVat::Fill to a reference: 7 uses and 3 calls changed\.$',
		 r'^Status: Converted the parameter psLevel of CVat::Sum to a reference: 3 uses and 2 calls changed\.$',
		 r'^Status: Converted the parameter sLevel of CVat::Drain to a pointer: 3 uses and 2 calls changed\.$',
		 r'^19\|\tvirtual\tint\t\tFill\(SLevel& psLevel, int iAmount\);$', r'^20\|\t\t\tint\t\tDrain\(const SLevel\* sLevel\);$', r'^21\|\t\t\tint\t\tCheck\(SLevel\* psLevel\);$',
		 r'^23\|\t\t\tint\t\tSum\(SLevel& psLevel, int iTimes\);$', r'^30\|\tint\t\tFill\(SLevel& psLevel, int iAmount\) override;$',
		 r'^30\|int CVat::Fill\(SLevel& psLevel, int iAmount\)$', r'^32\|\tpsLevel\.miDepth \+= iAmount;$', r'^33\|\tpsLevel\.miMark = Depth\(&psLevel\);$', r'^34\|\tmsLevel = psLevel;$',
		 r'^43\|int CDeepVat::Fill\(SLevel& psLevel, int iAmount\)$', r'^46\|\treturn CVat::Fill\(psLevel, 0\);$',
		 r'^54\|int CVat::Drain\(const SLevel\* sLevel\)$', r'^58\|\tpsSame = sLevel;$', r'^59\|\tmiCount = sLevel->miDepth \+ Depth\(&msLevel\);$', r'^60\|\tmsLevel = \*sLevel;$',
		 r'^100\|\treturn psLevel\.miMark \+ Sum\(psLevel, iTimes - 1\);$',
		 r'^119\|\tiTotal = pcVat->Fill\(sLevel, 1\);$', r'^120\|\tiTotal \+= pcDeep->Fill\(\*psMine, 2\);$', r'^121\|\tiTotal \+= pcVat->Drain\(&sLevel\);$', r'^122\|\tiTotal \+= pcVat->Drain\(psMine\);$',
		 r'^127\|\treturn iTotal \+ pcVat->Sum\(\*psLate, 2\);$'],
		[r"^Status: Can't"]),
	('pointer-local', dict(MakePointersCopy(), **{
		#Refused: a pointer a call gives NULL, one that is stepped, a local variable with no value where it is
		#declared, and what is no pointer.  A local pointer made a reference (its initialiser's & goes) and a local
		#reference made a pointer; then a function's parameter, whose calls with those change to match.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:69:26,edit:pointertoreference,edit:caret:83:26,edit:pointertoreference,edit:caret:118:11,edit:pointertoreference,edit:caret:112:11,edit:referencetopointer,edit:caret:111:11,edit:pointertoreference,edit:caret:114:8,edit:pointertoreference,edit:caret:20:20,edit:pointertoreference,printdisklines:Vat.h,printdisklines:Vat.cpp'}),
		[r"^Status: Can't convert the parameter psLevel of CVat::Check to a reference: it is given NULL \(Vat\.cpp line 123\)\.$",
		 r"^Status: Can't convert the parameter psLevel of CVat::Step to a reference: it is stepped: a reference always refers to one thing \(Vat\.cpp line 85\)\.$",
		 r'^Status: Converted the variable psLocal to a reference: 3 uses changed\.$', r'^Status: Converted the variable sOther to a pointer: 3 uses changed\.$',
		 r"^Status: Can't convert the variable psMine to a reference: it isn't given a value where it is declared: a reference must be\.$",
		 r"^Status: Can't convert the variable iTotal to a reference: it isn't a pointer\.$", r'^Status: Converted the parameter psLevel of Depth to a reference: 1 use and 4 calls changed\.$',
		 r'^34\|int\t\tDepth\(SLevel& psLevel\);$', r'^20\|int Depth\(SLevel& psLevel\)$', r'^22\|\treturn psLevel\.miDepth;$', r'^33\|\t\(\*psLevel\)\.miMark = Depth\(\*psLevel\);$',
		 r'^59\|\tmiCount = sLevel\.miDepth \+ Depth\(msLevel\);$', r'^69\|int CVat::Check\(SLevel\* psLevel\)$', r'^85\|\tpsLevel\+\+;$',
		 r'^112\|\tSLevel\*\tsOther = &pcVat->msLevel;$', r'^118\|\tSLevel&\tpsLocal = pcVat->msLevel;$',
		 r'^124\|\tiTotal \+= psLocal\.miDepth \+ psLocal\.miMark \+ Depth\(psLocal\);$', r'^125\|\tsOther->miDepth = iTotal;$', r'^126\|\tiTotal \+= Depth\(\*sOther\) \+ sOther->miMark;$'],
		[]),
	('pointer-field', dict(MakePointersCopy(), **{
		#A field that is a reference made a pointer: its declaration, what its constructor binds it to, and its uses,
		#alone, through this and through other objects.  A field that is a pointer isn't made a reference.
		'WAIT_INDEX': '1', 'OPEN': 'Probe.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:10:11,edit:pointertoreference,edit:caret:9:11,edit:contextactions,edit:referencetopointer,printdisklines:Probe.h,printdisklines:Probe.cpp'}),
		[r"^Status: Can't convert the field mpsSpare to a reference: a field that is a reference must be bound by every constructor, and can't be set in Init or assigned again\.$",
		 r'^Context actions: Rename\.\.\. \| Convert Reference to Pointer \| Encapsulate Field\.\.\. \| Safe Delete\.\.\.$',
		 r'^Status: Converted the field msLevel to a pointer: 9 uses changed\.$', r'^9\|\tSLevel\*\tmsLevel;$',
		 r'^8\|CProbe::CProbe\(SLevel& sLevel\) : msLevel\(&sLevel\), miCount\(0\)$', r'^20\|\treturn msLevel->miDepth \+ Mark\(msLevel\) \+ mpsSpare->miMark;$',
		 r'^30\|\tmsLevel->miDepth = iDepth;$', r'^31\|\tthis->msLevel->miMark = iDepth;$', r'^44\|\tsCopy = \*pcProbe->msLevel;$',
		 r'^45\|\tpcProbe->msLevel->miMark = cOther\.msLevel->miDepth \+ sCopy\.miMark;$', r'^46\|\treturn Mark\(pcProbe->msLevel\) \+ \(cOther\.msLevel\)->miMark;$'],
		[]),
	('qualifier-const', dict(MakeQualifiersCopy(), **{
		#Make Member Function const: const after its parameters in its declaration and definition, and in the function
		#that overrides it.  What a body does that a const function can't is a conflict and nothing is changed: a
		#field assigned (an array's element, a struct's member), a call of a member that isn't const (one made const
		#since is no conflict).  What a pointer field points to may be changed.  With a, it is made const whatever
		#the conflicts.  A function that is no member, and one that is const, are refused.
		'WAIT_INDEX': '1', 'OPEN': 'Meter.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:64:14,edit:makeconst,edit:caret:21:14,edit:makeconst,edit:caret:21:14,edit:makeconst,edit:caret:74:14,edit:makeconst,edit:caret:41:14,edit:makeconst,edit:caret:53:14,edit:makeconst,edit:caret:64:14,edit:makeconst,edit:caret:105:18,edit:makeconst:a,edit:caret:116:6,edit:makeconst,edit:open:Meter.h,edit:caret:21:10,edit:makeconst,printdisklines:Meter.h,printdisklines:Meter.cpp'}),
		[r"^Conflict: It calls Level, which isn't const, in Meter\.cpp at line 66\.$", r"^Conflict: It calls Raise, which isn't const, in Meter\.cpp at line 66\.$",
		 r'^Status: Made CMeter::Level const, and 1 function with it\.$', r"^Status: Can't make CMeter::Level const: it is const already\.$", r'^Status: Made CMeter::Next const\.$',
		 r'^Conflict: It assigns miLevel, in Meter\.cpp at line 43\.$', r'^Conflict: It assigns maiMarks, in Meter\.cpp at line 44\.$', r'^Conflict: It assigns msMark, in Meter\.cpp at line 55\.$',
		 r"^Conflict: It calls Raise, which isn't const, in Meter\.cpp at line 66\.$", r'^Conflict: It assigns miLevel, in Meter\.cpp at line 107\.$', r'^Status: Made CMeter::Find const\.$',
		 r"^Status: Can't make Use const: it isn't a member function\.$", r'^Status: Made CMeter::Top const\.$',
		 r'^20\|\tvirtual\tint\t\tLevel\(void\) const;$', r'^21\|\t\t\tint\t\tTop\(void\) const \{ return maiMarks\[3\]; \}$', r'^23\|\t\t\tint\t\tRaise\(int iBy\);$', r'^26\|\t\t\tint\t\tNext\(void\) const;$',
		 r'^29\|\t\t\tCMeter\*\tFind\(void\) const;$', r'^36\|\tint\t\tLevel\(void\) const override;$',
		 r'^21\|int CMeter::Level\(void\) const$', r'^31\|int CFineMeter::Level\(void\) const$', r'^41\|int CMeter::Raise\(int iBy\)$', r'^64\|int CMeter::Both\(void\)$', r'^74\|int CMeter::Next\(void\) const$',
		 r'^105\|CMeter\* CMeter::Find\(void\) const$'],
		[r'^Status: Made CMeter::(Raise|Mark|Both)']),
	('qualifier-static', dict(MakeQualifiersCopy(), **{
		#Make Member Function static: refused for a function whose body names a field, a virtual one and one static
		#already.  static goes before it in its class (in the column virtual has), a const after its parameters is
		#taken away, and the calls on an object name the class, but not the call on what a function gives.
		'WAIT_INDEX': '1', 'OPEN': 'Meter.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:95:14,edit:makestatic,edit:caret:21:14,edit:makestatic,edit:caret:105:18,edit:makestatic,edit:caret:85:14,edit:makestatic,edit:caret:85:14,edit:makestatic,edit:open:Meter.h,edit:caret:22:10,edit:makestatic,printdisklines:Meter.h,printdisklines:Meter.cpp'}),
		[r"^Status: Can't make CMeter::Twice static: its body uses its object: it names miLevel \(Meter\.cpp line 97\)\.$",
		 r"^Status: Can't make CMeter::Level static: it is virtual: a virtual function is called on an object\.$",
		 r"^Status: Can't make CMeter::Find static: its body uses its object: it names miLevel \(Meter\.cpp line 107\)\.$",
		 r'^Status: Made CMeter::Scale static: 2 calls name CMeter\.$', r"^Status: Can't make CMeter::Scale static: it is static already\.$", r'^Status: Made CMeter::Zero static: 1 call names CMeter\.$',
		 r'^22\|\tstatic\tint\t\tZero\(void\) \{ return 0; \}$', r'^27\|\tstatic\tint\t\tScale\(int iValue\);$', r'^85\|int CMeter::Scale\(int iValue\)$', r'^97\|\treturn Scale\(iValue\) \+ miLevel;$',
		 r'^122\|\tiTotal = CMeter::Scale\(2\);$', r'^123\|\tiTotal \+= CMeter::Scale\(3\);$', r'^124\|\tiTotal \+= pcMeter->Find\(\)->Scale\(4\);$', r'^125\|\tiTotal \+= CMeter::Zero\(\) \+ cMeter\.Top\(\);$'],
		[]),
	('member-from-function', dict(MakePointersCopy(), **{
		#Convert Function to Member Function: a function whose first parameter points to a struct becomes the
		#struct's.  It is declared there, its own declaration goes, its definition is SLevel::Depth without the
		#parameter, p->x in its body is x, and its calls name their object first: Depth(psLevel) is
		#psLevel->Depth(), Depth(&msLevel) msLevel.Depth().  And one whose first parameter points to a class: what
		#it named through the parameter it names alone.  Its other parameter's class is declared under the class
		#it goes to, so it is declared above it too ("class CDeepVat;"): the declaration wouldn't compile without.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:20:6,edit:functiontomember,edit:caret:108:6,edit:functiontomember,printdisklines:Vat.h,printdisklines:Vat.cpp'}),
		[r'^Status: Converted Depth to the member function SLevel::Depth: 4 calls changed\.$', r'^Status: Converted Use to the member function CVat::Use: 0 calls changed\.$',
		 r'^8\|\tint\t\tmiMark;$', r'^9\|\tint\t\tDepth\(void\);$', r'^10\|\};$', r'^13\|class CDeepVat;$', r'^16\|class CVat$', r'^28\|\t\t\tint\t\tUse\(CDeepVat\* pcDeep\);$',
		 r'^20\|int SLevel::Depth\(void\)$', r'^22\|\treturn miDepth;$', r'^33\|\t\(\*psLevel\)\.miMark = psLevel->Depth\(\);$', r'^59\|\tmiCount = sLevel\.miDepth \+ msLevel\.Depth\(\);$',
		 r'^108\|int CVat::Use\(CDeepVat\* pcDeep\)$', r'^112\|\tSLevel&\tsOther = msLevel;$', r'^118\|\tSLevel\*\tpsLocal = &msLevel;$', r'^119\|\tiTotal = Fill\(&sLevel, 1\);$',
		 r'^120\|\tiTotal \+= pcDeep->Fill\(psMine, 2\);$', r'^124\|\tiTotal \+= psLocal->miDepth \+ \(\*psLocal\)\.miMark \+ psLocal->Depth\(\);$', r'^126\|\tiTotal \+= sOther\.Depth\(\) \+ sOther\.miMark;$',
		 r'^127\|\treturn iTotal \+ Sum\(psLate, 2\);$'],
		[r'^\d+\|int\t\tDepth\(SLevel\* psLevel\);$']),
	('member-to-function', dict(MakePointersCopy(), **{
		#Convert Member Function to Function: its class's declaration of it goes and it is declared after its class,
		#with a first parameter that points to the class; what its body names of its object is named through the
		#parameter, a call of itself gives the parameter on, and a call on an object gives the object.  Refused: a
		#virtual function, and what is no member function.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:94:12,edit:membertofunction,edit:caret:8:13,edit:membertofunction,edit:caret:30:12,edit:membertofunction,edit:caret:20:6,edit:membertofunction,printdisklines:Vat.h,printdisklines:Vat.cpp'}),
		[r'^Status: Converted CVat::Sum to the function Sum\(CVat\* pcVat, \.\.\.\): 2 calls changed\.$', r'^Status: Converted CVat::Init to the function Init\(CVat\* pcVat, \.\.\.\): 0 calls changed\.$',
		 r"^Status: Can't convert CVat::Fill to a function: it is virtual, or a template's body calls it on an object\.$",
		 r"^Status: Can't convert Depth to a function: it isn't a member function of a class \(a class template's can't be\)\.$",
		 r'^18\|\tvirtual\tint\t\tFill\(SLevel\* psLevel, int iAmount\);$', r'^\d+\|void\tInit\(CVat\* pcVat\);$', r'^\d+\|int\tSum\(CVat\* pcVat, SLevel\* psLevel, int iTimes\);$', r'^\d+\|class CDeepVat : public CVat$',
		 r'^8\|void Init\(CVat\* pcVat\)$', r'^10\|\tpcVat->msLevel\.miDepth = 0;$', r'^12\|\tpcVat->miCount = 0;$', r'^94\|int Sum\(CVat\* pcVat, SLevel\* psLevel, int iTimes\)$',
		 r'^100\|\treturn psLevel->miMark \+ Sum\(pcVat, psLevel, iTimes - 1\);$', r'^127\|\treturn iTotal \+ Sum\(pcVat, psLate, 2\);$'],
		[r'^\d+\|\t+void\tInit\(void\);$', r'^\d+\|\t+int\t\tSum\(SLevel\* psLevel, int iTimes\);$']),
	('template-parameter', dict(MakeTemplatesCopy(), **{
		#Extract Template Parameter: the type at the caret becomes a parameter of its function, template<class M>
		#over each declaration and definition.  A function a header declares has its definition put there (a
		#member's after its class, with its banner); one of its file alone stays.  The calls stay where the type is
		#in the parameters, and name it where it isn't (Make<CGauge>).  A type of the type's own gets typename.
		#int is a type too.  Refused: a virtual function, a parameter name the function has, what is no type.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:90:6,edit:templateparameter,edit:caret:77:12,edit:templateparameter,edit:extracttemplate:M,edit:caret:67:19,edit:extracttemplate:M,edit:caret:57:20,edit:templateparameter,edit:caret:47:18,edit:extracttemplate:iTimes,edit:extracttemplate:G,edit:caret:34:13,edit:extracttemplate:M,edit:caret:23:3,edit:templateparameter,edit:extracttemplate:M,edit:caret:8:12,edit:templateparameter,edit:extracttemplate:M,printdisklines:Mix.h,printdisklines:Mix.cpp'}),
		[r'^Extract Template Parameter: no type in a function$', r"^Extract Template Parameter: int in Twice, the calls' arguments say it$", r'^Status: Extracted the template parameter M from int in Twice\.$',
		 r'^Status: Extracted the template parameter M from CGauge in Local\.$',
		 r"^Extract Template Parameter: CGauge can't be: its function is virtual, or a template's body calls it with other classes' functions: a virtual function can't be a template$",
		 r"^Extract Template Parameter: The function has something of the parameter's name already\.$",
		 r'^Status: Extracted the template parameter G from CGauge in CMixer::Sum: its definition is in Mix\.h now\.$', r'^Status: Extracted the template parameter M from CGauge in Marked: its definition is in Mix\.h now\.$',
		 r'^Extract Template Parameter: CGauge in Make, the calls must name it$', r'^Status: Extracted the template parameter M from CGauge in Make: its definition is in Mix\.h now, and 1 call names the type\.$',
		 r'^Status: Extracted the template parameter M from CGauge in Total: its definition is in Mix\.h now\.$',
		 r'^24\|\ttemplate<class G>$', r'^25\|\tint\t\tSum\(G\* pcGauge, int iTimes\);$', r'^26\|\tvirtual\tint\t\tBlend\(CGauge\* pcGauge\);$',
		 r'^\d+\|//////////////////////////////////////////////////////////////////////////$', r'^34\|template<class G>$', r'^35\|int CMixer::Sum\(G\* pcGauge, int iTimes\)$',
		 r'^\d+\|template<class M>$', r'^\d+\|int Total\(M\* pcGauge, int iCount\)$', r'^\d+\|\tM\*\tpcOther;$', r'^\d+\|\}$', r'^\d+\|$', r'^\d+\|$', r'^\d+\|template<class M>$', r'^\d+\|int Make\(int iSeed\)$', r'^\d+\|\tM\tcGauge;$',
		 r'^\d+\|int Marked\(M\* pcGauge\)$', r'^\d+\|\ttypename M::SMark\tsMark;$', r'^\d+\|#endif // __MIX_H__$',
		 r'^8\|int CMixer::Blend\(CGauge\* pcGauge\)$', r'^18\|template<class M>$', r'^19\|static int Local\(M\* pcGauge\)$', r'^29\|template<class M>$', r'^30\|M Twice\(M iValue\)$', r'^32\|\tM\t\tiResult;$',
		 r'^50\|\treturn Total\(&cGauge, 2\) \+ Make<CGauge>\(3\) \+ Marked\(&cGauge\) \+ cMixer\.Sum\(&cGauge, 2\) \+ cMixer\.Blend\(&cGauge\) \+ Local\(&cGauge\) \+ Twice\(4\);$'],
		[r'^\d+\|int\t\tTotal\(CGauge', r'^\d+\|int CMixer::Sum\(CGauge']),
	('template-parameter-dialog', dict(MakeTemplatesCopy(), **{
		#Its dialog asks the parameter's name.
		'WAIT_INDEX': '1', 'OPEN': 'Mix.cpp', 'SETTINGS': MakeSettingsFile(), 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'edit:caret:8:12,extracttemplateparameter,answered:1,printdisklines:Mix.h'}),
		[r'^Dialog \[Extract Template Parameter from CGauge\] answered Cancel$', r'^\d+\|int\t\tTotal\(CGauge\* pcGauge, int iCount\);$'],
		[r'^Status: Extracted']),
	('template-class', dict(MakeTemplatesCopy(), **{
		#Extract Template Parameter for a type a class's field has: the class becomes a class template of another
		#name, the type its parameter throughout it, its members' definitions (its constructor's and destructor's
		#too) come to its header after it as the template's, and its old name is a class that derives from the
		#template with the type it had and hands its constructor on, so what uses it stays as it is.  A class
		#declared ahead of its definition can be one too: its name is still a class, and its constructors are handed
		#on with their access, their parameters and their default values.  Refused: a parameter name the
		#class has, a template name that is taken.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:27:3,edit:templateparameter,edit:extracttemplate:N:CPipeTemplate,edit:caret:9:3,edit:templateparameter,edit:extracttemplate:miCount:CTankTemplate,edit:extracttemplate:M:CGauge,edit:extracttemplate:M:CTankTemplate,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r'^Extract Template Parameter: CGauge in the class CPipe, to be CPipeTemplate$',
		 r'^Status: Extracted the template parameter N from CGauge in CPipe: CPipe is a class that derives from CPipeTemplate<CGauge>\.$',
		 r'^Extract Template Parameter: CGauge in the class CTank, to be CTankTemplate$', r"^Extract Template Parameter: The class has something of the parameter's name already\.$",
		 r"^Extract Template Parameter: There is something of the template's name already\.$",
		 r'^Status: Extracted the template parameter M from CGauge in CTank: CTank is a class that derives from CTankTemplate<CGauge>, and 4 definitions are in Tank\.h now\.$',
		 r'^6\|template<class M>$', r'^7\|class CTankTemplate$', r'^10\|\tM\*\tmpcGauge;$', r'^11\|\tM\tmcSpare;$', r'^14\|\tCTankTemplate\(void\);$', r'^15\|\t~CTankTemplate\(void\);$',
		 r'^16\|\tvoid\tInit\(M\* pcGauge\);$', r'^18\|\tCTankTemplate\*\tSame\(void\) \{ return this; \}$', r'^19\|\};$',
		 r'^22\|//////////////////////////////////////////////////////////////////////////$', r'^26\|template<class M>$', r'^27\|CTankTemplate<M>::CTankTemplate\(void\)$',
		 r'^\d+\|CTankTemplate<M>::~CTankTemplate\(void\)$', r'^\d+\|void CTankTemplate<M>::Init\(M\* pcGauge\)$', r'^\d+\|int CTankTemplate<M>::Level\(void\)$', r'^\d+\|\tM\*\tpcGauge;$',
		 r'^71\|class CTank : public CTankTemplate<CGauge>$', r'^72\|\{$', r'^73\|public:$', r'^74\|\tCTank\(void\) : CTankTemplate<CGauge>\(\) \{\}$', r'^75\|\};$',
		 r'^78\|class CPipe;$', r'^81\|template<class N>$', r'^82\|class CPipeTemplate$', r'^\d+\|\tN\*\tmpcGauge;$', r'^\d+\|	CPipeTemplate\(N\* pcGauge, int iCount = 2\) \{ mpcGauge = pcGauge \+ iCount; \}$',
		 r'^\d+\|class CPipe : public CPipeTemplate<CGauge>$', r'^\d+\|\{$', r'^\d+\|public:$', r'^\d+\|	CPipe\(void\) : CPipeTemplate<CGauge>\(\) \{\}$', r'^\d+\|protected:$',
		 r'^\d+\|	CPipe\(CGauge\* pcGauge, int iCount = 2\) : CPipeTemplate<CGauge>\(pcGauge, iCount\) \{\}$', r'^\d+\|\};$',
		 r'^1\|#include "Tank\.h"$', r'^8\|int UseTank\(void\)$', r'^10\|\tCTank\tcTank;$', r'^16 lines$'],
		[r'^\d+\|[a-z ]*CTank::']),
	('template-remove', dict(MakeTemplatesCopy(), **{
		#Remove Template Parameter: a class template's parameter that every use gives one argument.  The only
		#parameter of a template: the class is a template no longer, its member defined outside it is inline, and
		#its uses lose their <...>.  One of two: it goes from each template<...>, from the member's CPair<A, B>:: and
		#from every use, and the argument is written where it was named.  Refused: a parameter given two arguments,
		#and what is no parameter.
		'WAIT_INDEX': '1', 'OPEN': 'Pair.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:45:22,edit:removetemplateparameter,edit:caret:6:26,edit:removetemplateparameter,edit:caret:6:17,edit:removetemplateparameter,edit:caret:7:8,edit:removetemplateparameter,printdisklines:Pair.h,printdisklines:Pair.cpp'}),
		[r'^Status: Removed the template parameter M from CLone: it is CGauge in the one place the template is used\.$',
		 r"^Status: Can't remove the template parameter B: It isn't given one argument everywhere: int and float \(Pair\.cpp line 11\)\.$",
		 r'^Status: Removed the template parameter A from CPair: it is CGauge in all 2 places the template is used\.$',
		 r"^Status: Can't remove the template parameter CPair: The caret isn't on a template's parameter\.$",
		 r'^6\|template<class B>$', r'^7\|class CPair$', r'^10\|\tCGauge\*\t\tmpFirst;$', r'^11\|\tB\t\tmSecond;$', r'^13\|\tvoid\tSet\(CGauge\* pFirst, B second\);$',
		 r'^22\|template<class B>$', r'^23\|void CPair<B>::Set\(CGauge\* pFirst, B second\)$', r'^30\|class CLone$', r'^33\|\tCGauge\*\t\tmp;$', r'^35\|\tvoid\tHold\(CGauge\* p\);$',
		 r'^43\|inline void CLone::Hold\(CGauge\* p\)$',
		 r'^10\|\tCPair<int>\t\tcPair;$', r'^11\|\tCPair<float>\tcOther;$', r'^12\|\tCLone\t\t\tcLone;$'],
		[r'template<class M>', r'class A']),
	('context-actions', dict(MakePointersCopy(), **{
		#Show Context Actions (Alt+Enter): the refactorings for what the caret is on.  A member function's
		#definition; a function that is no class's; a parameter that is a pointer, the type it is declared with, and
		#one that is a reference; a field where it is used; a number; an #include; a comment, where there is nothing.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:caret:8:12,edit:contextactions,edit:caret:20:6,edit:contextactions,edit:caret:30:24,edit:contextactions,edit:caret:30:17,edit:contextactions,'
				 'edit:caret:54:32,edit:contextactions,edit:caret:12:3,edit:contextactions,edit:caret:10:20,edit:contextactions,edit:caret:1:5,edit:contextactions,'
				 'edit:caret:5:2,edit:contextactions'}),
		[r'^Context\ actions:\ Rename\.\.\.\ \|\ Change\ Signature\.\.\.\ \|\ Inline\.\.\.\ \|\ Move\ Definition\ into\ Class\ \|\ Make\ Member\ Function\ const\ \|\ Make\ Member\ Function\ static\ \|\ Convert\ Member\ Function\ to\ Function\ \|\ Push\ Members\ Down\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Change\ Signature\.\.\.\ \|\ Unify\ Parameter\ Names\ \|\ Inline\.\.\.\ \|\ Move\ Definition\ into\ Class\ \|\ Convert\ Function\ to\ Member\ Function\ \|\ Move\.\.\.\ \|\ Move\ to\ Namespace\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Convert\ Pointer\ to\ Reference$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Move\.\.\.\ \|\ Copy\ Class\.\.\.\ \|\ Move\ to\ Namespace\.\.\.\ \|\ Extract\ Superclass\.\.\.\ \|\ Extract\ Interface\.\.\.\ \|\ Extract\ Class\.\.\.\ \|\ Extract\ Template\ Parameter\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Convert\ Reference\ to\ Pointer$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Encapsulate\ Field\.\.\.$',
		 r'^Context\ actions:\ Introduce\ Variable\.\.\.\ \|\ Introduce\ Field\.\.\.\ \|\ Introduce\ Constant\.\.\.\ \|\ Introduce\ Parameter\.\.\.$',
		 r'^Context\ actions:\ Optimise\ \#includes$',
		 r'^Context\ actions:\ none$'],
		[]),
	('context-actions-selection', dict(MakePointersCopy(), **{
		#What is selected: an expression in a function's body, whole statements, and a type.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:selecttext:33:22-33:36,edit:contextactions,edit:selecttext:32:2-33:37,edit:contextactions,edit:selecttext:56:2-56:15,edit:contextactions,'
				 'edit:selecttext:33:25-33:30,edit:contextactions'}),
		[r'^Context\ actions:\ Introduce\ Variable\.\.\.\ \|\ Introduce\ Field\.\.\.\ \|\ Introduce\ Constant\.\.\.\ \|\ Introduce\ Parameter\.\.\.\ \|\ Extract\ Method\.\.\.$',
		 r'^Context\ actions:\ Extract\ Method\.\.\.$',
		 r'^Context\ actions:\ Introduce\ Typedef\.\.\.$',
		 r'^Context\ actions:\ none$'],
		[]),
	('context-actions-class', dict(MakePointersCopy(), **{
		#A class with classes that derive from it, and one that derives; a member function declared in its class: one
		#that is virtual, one that overrides and one that is neither.  A struct's field.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.h',
		'DEBUG': 'edit:caret:12:8,edit:contextactions,edit:caret:27:8,edit:contextactions,edit:caret:19:16,edit:contextactions,edit:caret:30:8,edit:contextactions,'
				 'edit:caret:20:14,edit:contextactions,edit:caret:7:8,edit:contextactions,edit:caret:34:8,edit:contextactions'}),
		[r'^Context\ actions:\ Rename\.\.\.\ \|\ Move\.\.\.\ \|\ Copy\ Class\.\.\.\ \|\ Move\ to\ Namespace\.\.\.\ \|\ Push\ Members\ Down\.\.\.\ \|\ Extract\ Superclass\.\.\.\ \|\ Extract\ Interface\.\.\.\ \|\ Extract\ Class\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Move\.\.\.\ \|\ Copy\ Class\.\.\.\ \|\ Move\ to\ Namespace\.\.\.\ \|\ Pull\ Members\ Up\.\.\.\ \|\ Extract\ Superclass\.\.\.\ \|\ Extract\ Interface\.\.\.\ \|\ Extract\ Class\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Change\ Signature\.\.\.\ \|\ Unify\ Parameter\ Names\ \|\ Inline\.\.\.\ \|\ Move\ Definition\ into\ Class\ \|\ Make\ Member\ Function\ const\ \|\ Push\ Members\ Down\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Change\ Signature\.\.\.\ \|\ Unify\ Parameter\ Names\ \|\ Inline\.\.\.\ \|\ Move\ Definition\ into\ Class\ \|\ Make\ Member\ Function\ const\ \|\ Pull\ Members\ Up\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Change\ Signature\.\.\.\ \|\ Unify\ Parameter\ Names\ \|\ Inline\.\.\.\ \|\ Move\ Definition\ into\ Class\ \|\ Make\ Member\ Function\ const\ \|\ Make\ Member\ Function\ static\ \|\ Convert\ Member\ Function\ to\ Function\ \|\ Push\ Members\ Down\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Encapsulate\ Field\.\.\.\ \|\ Safe\ Delete\.\.\.$',
		 r'^Context\ actions:\ Rename\.\.\.\ \|\ Change\ Signature\.\.\.\ \|\ Unify\ Parameter\ Names\ \|\ Inline\.\.\.\ \|\ Move\ Definition\ into\ Class\ \|\ Convert\ Function\ to\ Member\ Function\ \|\ Move\.\.\.\ \|\ Move\ to\ Namespace\.\.\.\ \|\ Safe\ Delete\.\.\.$'],
		[]),
	('context-actions-macro', dict(MakeMacrosCopy(), **{
		#A macro where it is used and where it is #defined.
		'WAIT_INDEX': '1', 'OPEN': 'Use.cpp',
		'DEBUG': 'edit:caret:39:17,edit:contextactions,edit:open:Macros.h,edit:caret:6:10,edit:contextactions'}),
		[r'^Context\ actions:\ Inline\.\.\.\ \|\ Convert\ Macro\.\.\.$',
		 r'^Context\ actions:\ Inline\.\.\.\ \|\ Convert\ Macro\.\.\.$'],
		[]),
	('context-actions-popup', dict(MakePointersCopy(), **{
		#The command shows them in a popup under the caret, and the one chosen (Down, Enter) is run: the pointer
		#parameter is made a reference.  Where there is nothing the status line says so.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:30:24,command:Refactor.ContextActions,edit:printpopup,edit:key:down,edit:key:enter,edit:printpopup,edit:caret:5:2,command:Refactor.ContextActions,printstatus'}),
		[r'^Popup:\ Rename\.\.\.\ \|\ Convert\ Pointer\ to\ Reference$',
		 r'^Status:\ Converted\ the\ parameter\ psLevel\ of\ CVat::Fill\ to\ a\ reference:\ 7\ uses\ and\ 3\ calls\ changed\.$',
		 r'^Popup:\ none$',
		 r'^Status:\ There\ is\ nothing\ to\ refactor\ at\ the\ caret\.$'],
		[]),
	('context-actions-key', dict(MakePointersCopy(), **{
		#Alt+Enter in the editor, pressed as the keyboard's keys: the popup opens.
		'WAIT_INDEX': '1', 'OPEN': 'Vat.cpp',
		'DEBUG': 'edit:caret:54:32,wait:300,edit:press:Left Alt,edit:tap:Enter,edit:release:Left Alt,wait:300,edit:printpopup,edit:key:escape,edit:printpopup,edit:print'}),
		[r'^Popup: Rename\.\.\. \| Convert Reference to Pointer$', r'^Popup: none$'],
		[]),
	('index-template-itself', dict(MakeTemplatesCopy(), **{
		#The index checks a template in its own words: what the body of a template nothing instantiates names is a
		#usage (CRack<M>::Level calls CGauge::Read).
		'WAIT_INDEX': '1', 'OPEN': 'Mix.h', 'CARET': '15:8', 'EDIT': 'usages'}),
		[r'^Usages of Read  11 usages$', r'^      Rack\.h  1$', r'^        CRack  1$', r'^          14: int  Level\(void\) \{ return mcGauge\.Read\(\); \}$'],
		[]),
	('template-fold', dict(MakeTemplatesCopy(), **{
		#Extract Template Parameter on a class and Remove Template Parameter after it: the class is a template no
		#longer, and takes its name back from the class that was its name, which goes.
		'WAIT_INDEX': '1', 'OPEN': 'Tank.h',
		'DEBUG': 'edit:renamesearch:---s,edit:caret:9:3,edit:extracttemplate:M:CTankTemplate,edit:caret:6:17,edit:removetemplateparameter,printdisklines:Tank.h,printdisklines:Tank.cpp'}),
		[r'^Status: Removed the template parameter M from CTankTemplate: it is CGauge in all 2 places the template is used\.  It is CTank again, which was the class that was its name\.$',
		 r'^6\|class CTank$', r'^9\|\tCGauge\*\tmpcGauge;$', r'^13\|\tCTank\(void\);$', r'^14\|\t~CTank\(void\);$', r'^17\|\tCTank\*\tSame\(void\) \{ return this; \}$', r'^18\|\};$',
		 r'^25\|inline CTank::CTank\(void\)$', r'^36\|inline CTank::~CTank\(void\)$', r'^46\|inline void CTank::Init\(CGauge\* pcGauge\)$', r'^66\|class CPipe;$', r'^80 lines$',
		 r'^10\|\tCTank\tcTank;$'],
		[r'CTankTemplate\b.*\n(.*\n)*Disk lines Tank\.cpp', r'^\d+\|.*CTankTemplate']),
	('rename-assembly-class', dict(MakeRenameAssemblyCopy(), **{
		#A class renamed in hand written assembly's labels (_CEngine__Init, __0, __2), in parameter lists with its length
		#(FPC7CEngine to FPC6CMotor), in generated layout constants (SIZEOF_CEngine, CEngine_miSpeed: in the assembly, the
		#.inc and a C++ string) and in comments; not CEngineCache's labels or constants, or an assembly string.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '5:8',
		'DEBUG': 'edit:renamesearch:ct-s,edit:rename:CMotor,printdiskfile:EngineW65C816.asm,printdiskfile:EngineLayout.inc,printdiskfile:Engine.cpp'}),
		[r'^Status: Renamed CEngine to CMotor: .*, 15 in assembly in 4 files\.$', r"^; CMotor's functions in assembly", r'^_CMotor__0:$', r'^_CMotor__Init:$',
		 r'^\t\tjsr _CEngineCache__Init$', r'^_CMotor__Run__Fi:$', r'^_CMotor__Mix__Fi:$', r'^\t\tlda CMotor_miSpeed,x$', r'^\t\tldy #SIZEOF_CMotor$', r'^_CPart__Attach__FPC6CMotor:$', r'^_EngineCount__FPC6CMotor:$', r'^\t\tlda #<_CMotor__2$',
		 r'^_CEngineCache__Init:$', r'^\t\t\.byte "CEngine", 0$',
		 r'^CMotor_miSpeed = 0x0$', r'^SIZEOF_CMotor = 0x2$', r'^CEngineCache_miSpeed = 0x0$', r'^SIZEOF_CEngineCache = 0x2$',
		 r'^const char\* gszEngineSpeed = "CMotor_miSpeed";$'],
		[r'_CEngine__', r'7CEngine', r'CMotorCache', r'CEngine_miSpeed', r'SIZEOF_CEngine\b']),
	('rename-assembly-field', dict(MakeRenameAssemblyCopy(), **{
		#A field renamed in generated layout constants, its class's (CEngine_miSpeed) not another's (CEngineCache_miSpeed).
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '8:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:miPace,printdiskfile:EngineW65C816.asm,printdiskfile:EngineLayout.inc'}),
		[r'^Status: Renamed miSpeed to miPace: .*, 2 in assembly in 4 files\.$', r'^\t\tlda CEngine_miPace,x$', r'^CEngine_miPace = 0x0$', r'^CEngineCache_miSpeed = 0x0$'],
		[r'CEngine_miSpeed', r'CEngineCache_miPace']),
	('rename-assembly-member', dict(MakeRenameAssemblyCopy(), **{
		#A member function: _CEngine__Init, not _CEngineCache__Init.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '10:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:Start,printdiskfile:EngineW65C816.asm'}),
		[r'^Status: Renamed Init to Start: .*, 2 in assembly in 3 files\.$', r'^_CEngine__Start:$', r'^\t\tjsr _CEngineCache__Init$', r'^\t\tjsr _CEngine__Start$',
		 r'^_CEngineCache__Init:$'], [r'_CEngine__Init', r'_CEngineCache__Start']),
	('rename-assembly-overloaded', dict(MakeRenameAssemblyCopy(), **{
		#An overloaded member: the label whose parameters are Run(int)'s (Fi) and no other overload's, not Run(char)'s (Fc).
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '11:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:Go,printdiskfile:EngineW65C816.asm'}),
		[r'^Status: Renamed Run to Go: .*, 1 in assembly in 3 files\.$', r'^_CEngine__Go__Fi:$', r'^_CEngine__Run__Fc:$'], [r'_CEngine__Go__Fc', r'check by hand']),
	('rename-assembly-overload-unsure', dict(MakeRenameAssemblyCopy(), **{
		#A label that fits two overloads (Fi is Mix(int)'s and Mix(long)'s, both 4 bytes) is left, and the status line says so.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '13:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:Blend,printdiskfile:EngineW65C816.asm'}),
		[r'^Status: Renamed Mix to Blend: .*1 assembly label fits this overload and another: left, check by hand\.$', r'^_CEngine__Mix__Fi:$'],
		[r'_CEngine__Blend']),
	('rename-assembly-function', dict(MakeRenameAssemblyCopy(), **{
		#A function outside a class: its label, its parameter list as it was.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '32:6', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:MotorCount,printdiskfile:EngineW65C816.asm'}),
		[r'^Status: Renamed EngineCount to MotorCount: .*, 1 in assembly in 3 files\.$', r'^_MotorCount__FPC7CEngine:$'], [r'_EngineCount__']),
	('rename-assembly-namespace', dict(MakeRenameAssemblyCopy(), **{
		#A namespace: a part of labels (_NGear__Spin) and of length-prefixed names, the lengths of the names they are in
		#with them (C23CBox__T_C12NGear__CGear to C22CBox__T_C11NCog__CGear).
		'WAIT_INDEX': '1', 'OPEN': 'Gear.h', 'CARET': '5:12', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:NCog,printdiskfile:GearW65C816.asm'}),
		[r'^Status: Renamed NGear to NCog: .*, 9 in assembly in \d+ files\.$', r"^; NCog's and CBox's functions", r'^_NCog__CGear__Turn:$', r'^_NCog__Spin__FPC11NCog__CGear:$',
		 r'^\t\tjsr _NCog__CGear__Turn$', r'^_CBox__T_C11NCog__CGear__Fill:$', r'^_CBox__T_i__Fill:$', r'^_Pack__FPC22CBox__T_C11NCog__CGear:$',
		 r'^\t\tjsr _CBox__T_C11NCog__CGear__Fill$', r'^_Load__FPC9CBox__T_i:$'],
		[r'_NGear', r'NGear__']),
	('rename-assembly-namespace-class', dict(MakeRenameAssemblyCopy(), **{
		#A class in a namespace, in a template argument too.
		'WAIT_INDEX': '1', 'OPEN': 'Gear.h', 'CARET': '7:9', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:CCog,printdiskfile:GearW65C816.asm'}),
		[r'^Status: Renamed CGear to CCog: .*, 6 in assembly in \d+ files\.$', r'^_NGear__CCog__Turn:$', r'^_NGear__Spin__FPC11NGear__CCog:$',
		 r'^\t\tjsr _NGear__CCog__Turn$', r'^_CBox__T_C11NGear__CCog__Fill:$', r'^_Pack__FPC22CBox__T_C11NGear__CCog:$', r'^\t\tjsr _CBox__T_C11NGear__CCog__Fill$'],
		[r'__CGear']),
	('rename-assembly-template', dict(MakeRenameAssemblyCopy(), **{
		#A class template: its instances' labels (_CBox__T_i) and their length-prefixed names.
		'WAIT_INDEX': '1', 'OPEN': 'Gear.h', 'CARET': '20:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:CCrate,printdiskfile:GearW65C816.asm'}),
		[r'^Status: Renamed CBox to CCrate: .*, 8 in assembly in \d+ files\.$', r"^; NGear's and CCrate's functions", r'^_CCrate__T_C12NGear__CGear__Fill:$', r'^_CCrate__T_i__Fill:$',
		 r'^_Pack__FPC25CCrate__T_C12NGear__CGear:$', r'^\t\tjsr _CCrate__T_C12NGear__CGear__Fill$', r'^_Load__FPC11CCrate__T_i:$', r'^\t\tjsr _CCrate__T_i__Fill$', r'^\t\tjsr _Load__FPC11CCrate__T_i$'],
		[r'CBox__']),
	('rename-assembly-template-member', dict(MakeRenameAssemblyCopy(), **{
		#A class template's member: its label in every instance, after the instance's template arguments.
		'WAIT_INDEX': '1', 'OPEN': 'Gear.h', 'CARET': '25:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:Stuff,printdiskfile:GearW65C816.asm'}),
		[r'^Status: Renamed Fill to Stuff: .*, 4 in assembly in \d+ files\.$', r'^_CBox__T_C12NGear__CGear__Stuff:$', r'^_CBox__T_i__Stuff:$',
		 r'^\t\tjsr _CBox__T_C12NGear__CGear__Stuff$', r'^\t\tjsr _CBox__T_i__Stuff$'],
		[r'__Fill']),
	('rename-assembly-namespace-function', dict(MakeRenameAssemblyCopy(), **{
		#A function in a namespace: _NGear__Spin and its parameters.
		'WAIT_INDEX': '1', 'OPEN': 'Gear.h', 'CARET': '15:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:Whirl,printdiskfile:GearW65C816.asm'}),
		[r'^Status: Renamed Spin to Whirl: .*, 1 in assembly in \d+ files\.$', r'^_NGear__Whirl__FPC12NGear__CGear:$'], [r'__Spin']),
	('rename-assembly-extern-c', dict(MakeRenameAssemblyCopy(), **{
		#A function declared inside extern "C" in a namespace: its label is its name alone (_GearCount), not the namespace's
		#and parameters' (_NGear__GearCount__Fi, as a C++ function's would be).
		'WAIT_INDEX': '1', 'OPEN': 'Gear.h', 'CARET': '37:8', 'DEBUG': 'edit:renamesearch:ct-s,edit:rename:CogCount,printdiskfile:GearW65C816.asm'}),
		[r'^Status: Renamed GearCount to CogCount: .*, 2 in assembly in \d+ files\.$', r'^_CogCount:$', r'^_NGear__GearCount__Fi:$', r'^\t\tjsr _CogCount$'],
		[r'^_GearCount:', r'_NGear__CogCount']),
	('rename-file-refused', dict(MakeRenameFileCopy(), **{
		#A name that is taken, one with a directory, and the same name: nothing is renamed.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'DEBUG': 'edit:renamefile:EngineCache.h,edit:renamefile:Sub/Engine.h,edit:renamefile:Engine.h,printprojectfiles'}),
		[r"^Status: Can't rename: EngineCache\.h is already there\.$",
		 r"^Status: Can't rename: a file's new name can't be empty or have directories in it\.$",
		 r"^Status: Can't rename: Engine\.h is already called that\.$",
		 r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp .* Engine\.h EngineCache\.h'],
		[r'^Status: Renamed']),
	('index-parent-includes', {
		#Units reaching Engine.h with "../Engine.h" (Sub/User.cpp in its project's directory, Tool/Tool.cpp from outside
		#every include directory of its project) and "..\\Engine.h" (Sub/Indented.cpp) include it, as MSVC does: the
		#index has no problems, and has CEngine at its 4 places (the 5th is in code LINUX_GNU leaves out).
		'SOLUTION': RENAME_FILE, 'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'CARET': '5:8',
		'DEBUG': 'printindexing,edit:renamable,edit:usagegroups:,edit:usages'},
		[r'^Indexing: \d+ ms, 0 units with problems$', r'^Rename: CEngine \[CEngine\] at 4 places, 0 overrides$', r'^Usages of CEngine  3 usages$'],
		[r'Could not include']),
	('index-windows-headers', {
		#<windows.h> is read whole: on Linux MinGW's headers stand in for the Windows SDK's, and know the target by GCC's
		#name for it (__i386__), which winnt.h stopped with an #error without.
		'SOLUTION': WINDOWS_HEADERS, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'DEBUG': 'wait:1500,printindexing'},
		[r'^Indexing: \d+ ms, 0 units with problems$', r'^Indexing 1 files? \(Debug\|Win32\)$'], [r'#error', r'Could not include']),
	('index-windows-headers-x64', {
		#The same for x64 (__x86_64__).
		'SOLUTION': WINDOWS_HEADERS, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CONFIGURATION': 'Debug|x64', 'DEBUG': 'wait:1500,printindexing'},
		[r'^Indexing: \d+ ms, 0 units with problems$', r'^Indexing 1 files? \(Debug\|x64\)$'], [r'#error', r'Could not include']),
	('rename-file-includes-same-name', {
		#Other/Engine.h, a file with the same name, is found only by the #include next to it.
		'SOLUTION': RENAME_FILE, 'WAIT_INDEX': '1', 'OPEN': 'Other/Engine.h', 'EDIT': 'printincludes'},
		[r'^Includes of Engine\.h: 1$', r'^  OtherUser\.cpp 1:11 8$'], [r'Sub/', r'Tool/']),
	('rename-while-reindexing', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'int iTypedName; ', 'EDITS_DELAY': '1500',
		'DEBUG': 'wait:700,printedits,edit:caret:36:7,edit:renamable,printedits'},
		[r'^Edits thread: busy$',
		 r'^Rename: iTypedName \[.*CalculatorParser\.cpp:36:6:iTypedName\] at 1 places, 0 overrides$',
		 r'^Edits thread: idle$'], []),
	('recolour-off-ui-thread', {
		#Opened (not coloured yet) and typed in, given to the edits thread (held back), typed again while it is busy: what
		#it made is of the old text, so it isn't shown; the new text is given to it once it has finished, and shown.
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'int iTypedName; ', 'EDITS_DELAY': '5000',
		'DEBUG': 'printspans,wait:1500,printedits,edit:type:x,wait:5500,printedits,printspans,wait:5500,printedits,printspans,'
		         'edit:caret:36:7,edit:renamable'},
		[r'^Spans: same$', r'^Edits thread: busy$', r'^Edits thread: busy$', r'^Spans: same$', r'^Edits thread: idle$',
		 r'^Spans: changed$', r'^Rename: iTypedName \[.*CalculatorParser\.cpp:36:6:iTypedName\] at 1 places'], []),
	('header-edit-in-unit', {
		#A header opened is coloured a moment later, and its edits are worked out in the unit that includes it: its new
		#member is indexed as the class's.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:2', 'TYPE': 'int miTypedMember; ',
		'DEBUG': 'printspans,wait:1500,printedits,printspans,edit:caret:38:7,edit:renamable'},
		[r'^Spans: same$', r'^Edits thread: idle$', r'^Spans: changed$',
		 r'^Rename: miTypedMember \[CCalcParentheses::miTypedMember\] at 1 places, 0 overrides$'], []),
	('edits-kept-after-configuration', {
		#The index built again for another configuration has the open files' edits, not just their files.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'int iTypedName; ',
		'DEBUG': 'wait:1500,configuration:Debug|W65C816,waitindex,edit:caret:36:7,edit:renamable'},
		[r'^Rename: iTypedName \[.*CalculatorParser\.cpp:36:6:iTypedName\] at 1 places'], []),
	('nothing-at-caret', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '1:1', 'EDIT': 'usages,renamable'},
		[r'^Status: No usages found at the caret\.$', r'^Rename: nothing at the caret$'], []),
	('undo-out-of-view', {
		#Ctrl+Z in an editor undoes an edit in view at once; one scrolled out of view is shown by the first press, which
		#says so, and undone by the second.
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'X',
		'DEBUG': 'wait:300,edit:caret:400:1,wait:300,edit:keys:Control+Z,wait:300,edit:print,edit:keys:Control+Z,wait:300,edit:print'},
		[r'^Status: Ctrl\+Z again to undo the change in CalculatorParser\.cpp at line 36\.$', r'CalculatorParser\.cpp \* \(focus\) line 400: ',
		 r'CalculatorParser\.cpp \(focus\) line 36: \[\tmpcCalculator'], []),
	('undo-keys-across-files', {
		#Ctrl+Z in an editor undoes the other open files' edits too, newest first, as they were made: typed in one file,
		#then in another, the second's is undone, then the first's is shown and undone; Ctrl+Shift+Z redoes them.
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2',
		'DEBUG': 'wait:300,edit:type:X,edit:open:CalculatorSymbols.cpp,edit:type:Y,edit:keys:Control+Z,edit:print,edit:keys:Control+Z,edit:print,'
		         'edit:keys:Control+Z,edit:print,edit:keys:Control+Shift+Z,edit:keys:Control+Shift+Z,edit:keys:Control+Shift+Z,edit:print'},
		[r'CalculatorSymbols\.cpp \(focus\) line 1: \[#include "Logger\.h"\]', r'^Status: Ctrl\+Z again to undo the change in CalculatorParser\.cpp at line 36\.$',
		 r'CalculatorParser\.cpp \* \(focus\) line 36: \[\tXmpcCalculator', r'CalculatorParser\.cpp \(focus\) line 36: \[\tmpcCalculator',
		 r'^Status: Ctrl\+Shift\+Z again to redo the change in CalculatorSymbols\.cpp at line 1\.$',
		 r'CalculatorSymbols\.cpp \* \(focus\) line 1: \[Y#include'], []),
	('edit-menu-focus', {
		'OPEN': 'CalculatorParser.cpp', 'EDIT': 'focus:build,print,focus:claude,print'},
		[r'^Edit menu: Undo\(disabled\) Redo\(disabled\) Cut\(disabled\) Copy Paste\(disabled\) Paste from History\.\.\.\(disabled\) Select All',
		 r'^Edit menu: Undo\(disabled\) Redo\(disabled\) Cut Copy Paste Paste from History\.\.\. Select All'], []),
	('go-to-file', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'GOTO': '1:bl/csym'},
		[r'^Go to \[bl/csym\]: 2 results', r'^  CalculatorSymbols\.h ', r'^  CalculatorSymbols\.cpp ',
		 r'^Opened .*[/\\]CalculatorSymbols\.h$'], []),
	('go-to-keys-change-tab', {
		#With the dialog open its own keys change its tab, searching for what is typed in it.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Cancel', 'DIALOG_KEYS': 'Control+Shift+N,Control+Alt+Shift+N,Control+N',
		'DEBUG': 'goto:0:calcpars,answered:1'},
		[r'^Keys Control\+Shift\+N: taken$', r'^Go to tab 1 \[calcpars\]: 2 results$', r'^  CalculatorParser\.h ',
		 r'^Keys Control\+Alt\+Shift\+N: taken$', r'^Go to tab 2 \[calcpars\]: 1 results$', r'^  CCalculatorParser ',
		 r'^Keys Control\+N: taken$', r'^Go to tab 0 \[calcpars\]: 1 results$', r'^Focus: CTextField$'], [r'not taken']),
	('usages-row-parts', {
		#A usage's row has its line number dim, the usage's own text in the match font and the rest of its line's
		#keywords coloured; a group's count is dim, a file has its icon, and what is noted of a usage is dim.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,references,usageparts'},
		[r'^  CalcParentheses\.h\{d:  1\} \(icon\)$', r'^    \{d:38: \}\{k:bool\}  \{m:IsParentheses\}\(\{k:void\}\) \{k:const\} \{k:override\};\{d:  \[declaration\]\}$',
		 r'^  Calculator\.cpp\{d:  3\} \(icon\)$', r'^    \{d:391: \}\{k:if\} \(pcExpression->\{m:IsParentheses\}\(\)\)$'], [r'\{\?:', r'ERROR']),
	('usages-preview', {
		#Beside the tree the selected usage's file is shown at its line, coloured as an editor is; stepping to another
		#usage shows that; with the preview off nothing is shown.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,usagepreview,focus:usages', 'DEBUG': 'command:Navigate.NextOccurrence,edit:usagepreview,edit:usagepreview:off,edit:usagepreview,edit:usagepreview:on,edit:usagepreview'},
		[r'^Preview: Calculator\.cpp line 367 of \d+ \[if \(\(\*ppcExpression\)->IsParentheses\(\)\)\] coloured$',
		 r'^Preview: Calculator\.cpp line 391 of \d+ \[if \(pcExpression->IsParentheses\(\)\)\] coloured$', r'^Preview: hidden$',
		 r'^Preview: Calculator\.cpp line 391 of '], [r'Preview: none', r'ERROR']),
	('find-preview', {
		#The Find tool window has the same preview.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfindpreview'},
		[r'^Preview: CalculatorParser\.h line 29 of \d+ \[class CCalculatorParser\] coloured$'], [r'ERROR']),
	('usages-toolbar', {
		#The tool window's toolbar: Previous Occurrence is dim on the first usage; Next Occurrence steps and the tree keeps
		#the keys; a grouping's button turns the group on, and is on while it is; Preview and Navigate with Single Click
		#turn on and off; Rerun finds the usages again.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,usagetoolbar,usagetool:Next Occurrence,printselected,usagetoolbar,usagetool:Group by Project,usagetoolbar,printusages,'
		        'usagetool:Preview,usagetool:Navigate with Single Click,usagetoolbar,usagetool:Collapse All,printrows,usagetool:Expand All,printrows,usagetool:Rerun,printusages'},
		[r'^Toolbar: Rerun, Previous Occurrence\(disabled\), Next Occurrence, Expand All, Collapse All, Group by Usage Type, Group by Project, Group by Class, Show Read Access\(on\), Show Write Access\(on\), Preview\(on\), Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab, Scope$',
		 r'^Selected: 391: ', r'^Toolbar: Rerun, Previous Occurrence, Next Occurrence, ', r'Group by Project\(on\), Group by Class, Show Read Access\(on\), Show Write Access\(on\), Preview\(on\)',
		 r'^  BaseLib  3 usages$', r'Preview, Navigate with Single Click\(on\), Autoscroll from Source, Export\.\.\., Pin Tab, Scope$', r'^Rows: 1$', r'^Rows: 6$',
		 r'^Usages of IsParentheses and 1 override  3 usages$', r'^  BaseLib  3 usages$', r'^      391: '], [r'No tool button', r'ERROR']),
	('usages-tabs', {
		#Find Usages shows what it finds in the tab chosen unless it is pinned: then in a new tab, the pinned one kept.  A
		#tab that isn't pinned is used again, in its place.  A tab chosen shows its own usages; its cross closes it, pinned
		#or not.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,usagetabs,usagetool:Pin Tab,usagetoolbar,usagetabs,focus:editor,caret:37:14,usages,usagetabs,usagetoolbar,'
		        'focus:editor,caret:36:14,usages,usagetabs,usagetab:* IsParentheses,usagetabs,printusages,usageclosetab:* IsParentheses,usagetabs,printusages'},
		[r'^Usage tabs: >IsParentheses$', r'Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab\(on\), Scope$', r'^Usage tabs: >\* IsParentheses$',
		 r'^Usages of SetExpression', r'^Usage tabs: \* IsParentheses, >SetExpression$', r'Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab, Scope$',
		 r'^Usages of Evaluate', r'^Usage tabs: \* IsParentheses, >Evaluate$',
		 r'^Usage tabs: >\* IsParentheses, Evaluate$',
		 r'^Usages of IsParentheses and 1 override  3 usages$', r'^Usage tabs: >Evaluate$', r'^Usages of Evaluate'], [r'ERROR', r'SetExpression, .*Evaluate|Evaluate, .*SetExpression']),
	('usages-scope-current-file', {
		#Find Usages' scope: with Current File the usages in other files aren't in the tree, whose first row says the scope.
		#The symbol is declared in CalcParentheses.h and only used in Calculator.cpp.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usagescope:4,usages,usagescope:4,references'},
		[r'^No usages of IsParentheses and 1 override in CalcParentheses\.h$',
		 r'^References to IsParentheses and 1 override in CalcParentheses\.h  1 reference$', r'^    38: bool  IsParentheses\(void\) const override;'],
		[r'Calculator\.cpp', r'hidden', r'ERROR']),
	('usages-scope-project', {
		#The Project scope is the active file's project; Find All References uses the scope too.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usagescope:1,references'},
		[r'^References to IsParentheses and 1 override in BaseLib  7 references$', r'^  Calculator\.cpp  3$'], [r'ERROR']),
	('usages-scope-directory', {
		#A directory's scope: nothing of the symbol's is in a directory it isn't used in.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'FIND_DIRECTORY': os.path.join(ROOT, 'CommonLibraries', 'Libraries', 'WindowLib'),
		'EDIT': 'usagegroups:,usagescope:2,usages'},
		[r'^No usages of IsParentheses and 1 override in .*WindowLib$'], [r'Calculator\.cpp', r'ERROR']),
	('usages-settings-declarations', {
		#Find Usages (Alt+F7) has the declarations with the usages when its settings say so.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usagescope:0:d,usagesonly,printusages'},
		[r'^References to IsParentheses and 1 override  7 references$'], [r'ERROR']),
	('usages-settings-dialog', {
		#Find Usages Settings: its dialog, answered Find, finds the usages of the symbol at the caret.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'DIALOG_ANSWER': 'OK',
		'EDIT': 'usagegroups:', 'DEBUG': 'command:Navigate.FindUsagesSettings,answered:1,edit:printusages'},
		[r'^Dialog \[Find Usages Settings\] answered OK$', r'^Usages of IsParentheses and 1 override  3 usages$'], [r'ERROR']),
	('usages-scope-menu', {
		#The toolbar's Scope: a menu of the scopes (the startup solution's too: with one solution open it is that one), the tab's saying so; one chosen shows the usages already found that are
		#in it, as the IDE was when they were found, and choosing the solution's shows them all again.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,references,usagetool:Scope,usagescopemenu,usagescopemenu:3,printusages,usagetool:Scope,usagescopemenu,usagescopemenu:0,printusages'},
		[r'^References to IsParentheses and 1 override  7 references$', r'^Scope menu \(open\): Solution  \(shown\), Startup Solution, Project, Current File, Open Files, Directory$',
		 r'^References to IsParentheses and 1 override in CalcParentheses\.h  1 reference$',
		 r'^Scope menu \(open\): Solution, Startup Solution, Project, Current File  \(shown\), Open Files, Directory$',
		 r'^References to IsParentheses and 1 override  7 references$'], [r'ERROR']),
	('usages-template-arguments', dict(MakeUsagesKernel(
		'class CFoo { public: int miX; };\n'
		'template<class M> class CHolder { public: M* mp; };\n'
		'template<class M> M* New(void) { return 0; }\n'
		'template<class M = CFoo> class CDefault { };\n'
		'CHolder<CFoo> gcHolder;\n'
		'CHolder<CHolder<CFoo> > gcNested;\n'
		'class CDerived : public CHolder<CFoo> { };\n'
		'typedef CHolder<CFoo> HFoo;\n'
		'int main(void) { CFoo* p = New<CFoo>(); CHolder<CFoo*> c; return (int)sizeof(CHolder<CFoo>); }\n'), **{
		#Grouped by usage type, a class used in templates' arguments (a variable's type, nested, a base class's, a
		#typedef's, a function template's call, sizeof) and as a type parameter's default is in Usage in template
		#argument; the local it is the type of isn't.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:8', 'EDIT': 'usagegroups:t,usages,printusages'}),
		[r'^Usages of CFoo  9 usages$', r'^  Usage in local variable declaration  1 usage$', r'^  Usage in template argument  8 usages$',
		 r'^      4: template<class M = CFoo> class CDefault', r'^      5: CHolder<CFoo> gcHolder;$'], [r'Unclassified usage']),
	('problems-current-file', dict(MakeProblemsKernel(), **{
		#The Problems tool window's Current File: what the code model's check of the active editor's file found, errors
		#then warnings, each in its line's order with its place; its header's problem isn't the file's.  The editor
		#underlines each and marks its line beside the scroll bar, errors red and warnings amber.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:problems:file,edit:printproblemmarks'}),
		[r'^Problems tab: Current File$', r'^Kernel\.cpp  2 errors, 1 warning$', r'^  Only variables are supported by the compiler here yet \[giMissing\]  4:22$',
		 r'^  Expected an expression\. \[;\]  5:25$', r"^  The function overrides a virtual function of a base class but isn't declared override \[Get\]  3:45$",
		 r'^Problem marks: 2 error, 1 warning scroll marks$', r'^Underlined errors: \[giMissing\] \[;\]$', r'^Underlined warnings: \[Get\]$'],
		[r'giMissingInHeader', r'Problems\.h  ']),
	('problems-open-step', dict(MakeProblemsKernel(), **{
		#A problem opened goes to its place with the editor having the keys; Next Occurrence goes on from it, from the
		#editor, Problems being the tool window seen with something to step to.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:problems:file,edit:problemsopen:file:2,edit:print,command:Navigate.NextOccurrence,edit:print'}),
		[r'Kernel\.cpp \(focus\) line 5: \[int G\(void\) \{ return 1 \+; \}\]', r'Kernel\.cpp \(focus\) line 3: \[class CDerived'], [r'Nothing to step through']),
	('problems-fixed', dict(MakeProblemsKernel(), **{
		#Fixed in the editor, a problem goes once its edit has been checked, after a pause.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:problems:file,edit:replace:4|giMissing|0,wait:1500,edit:problems:file,edit:printproblemmarks'}),
		[r'^Kernel\.cpp  2 errors, 1 warning$', r'^Kernel\.cpp  1 error, 1 warning$', r'^Underlined errors: \[;\]$'], []),
	('problems-header', dict(MakeProblemsKernel(), **{
		#Another file made active, the tab shows its problems: the header's, checked in the unit that includes it.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'wait:1500,edit:open:Problems.h,wait:1500,edit:problems:file'}),
		[r'^Problems tab: Current File$', r'^Problems\.h  1 error$', r'^  Only variables are supported by the compiler here yet \[giMissingInHeader\]  1:22$'],
		[r'^Kernel\.cpp  ']),
	('problems-project', dict(MakeProblemsKernel(), **{
		#The Project tab: nothing until something is built; then the last build's errors and warnings by file (the
		#W65C816's, which can be more than the check's), and one opened goes to its place.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp',
		'DEBUG': 'edit:problems:project,command:Build.Build,wait:6000,edit:problems:project,edit:problemsopen:project:1,edit:print'}),
		[r'^Problems tab: Project$', r'^Nothing has been built yet$', r'^Problems tab: Project$', r'^Kernel\.cpp  \d+ errors, 1 warning$',
		 r"isn't declared override \[Get\]  3:45$", r'^Problems\.h  1 error$', r'Kernel\.cpp \(focus\) line \d+: '], [r'^Nothing has been built yet$.*\n.*Kernel']),
	('usages-access-filters', {
		#Show Read Access and Show Write Access, both on to start with: off, a field's usages that read it, or write it, are
		#left out of the tree, whose first row says how many are hidden.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.h', 'CARET': '32:17',
		'EDIT': 'usagegroups:t,usages,usagetoolbar,usagetool:Show Read Access,printusages,usagetoolbar,usagetool:Show Read Access,usagetool:Show Write Access,printusages'},
		[r'^Usages of mpcCalculator  18 usages$', r'^  Value read  16 usages$', r'^  Value write  2 usages$', r'Show Read Access\(on\), Show Write Access\(on\), Preview',
		 r'^Usages of mpcCalculator  18 usages  \(16 hidden\)$', r'^  Value write  2 usages$', r'^      36: mpcCalculator = pcCalculator;$',
		 r'Show Read Access, Show Write Access\(on\), Preview', r'^Usages of mpcCalculator  18 usages  \(2 hidden\)$', r'^  Value read  16 usages$'],
		[r'No tool button', r'ERROR']),
	('find-toolbar', {
		#The Find tool window's toolbar has no groupings, and its Replace buttons are for Replace in Files' results; Rerun
		#finds the text again.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfindtoolbar,findtool:Rerun,printfind'},
		[r'^Toolbar: Rerun, Previous Occurrence\(disabled\), Next Occurrence, Expand All, Collapse All, Replace All\(disabled\), Replace Selected\(disabled\), Preview\(on\), Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab$',
		 r"^Occurrences of 'calculatorparser' in the solution  13 matches in 2 files$"], [r'No tool button', r'ERROR']),
	('find-row-parts', {
		#The Find tool window's rows: the text found is in the match font whatever its case, and the rest of the line
		#is coloured as code where its own text says how: a directive, a string, keywords, a number.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfindparts'},
		[r'^  CalculatorParser\.h\{d:  1 match\} \(icon\)$', r'^    \{d:29: \}\{k:class\} C\{m:CalculatorParser\}$',
		 r'^    \{d:26: \}\{p:#include\} \{s:"\}\{m:CalculatorParser\}\{s:\.h"\}$',
		 r'^    \{d:35: \}memset\(\{k:this\}, \{n:0\}, \{k:sizeof\}\(C\{m:CalculatorParser\}\)\);$'], [r'\{\?:', r'ERROR']),
	('startup-project-default', dict(MakeToolsCopy(), **{
		#A solution with two applications: its first is its startup project, bold in Solution Explorer.
		'DEBUG': 'printstartup'}),
		[r'^Startup project of Tools: \[First\] of 2 applications$', r'^Bold project: First$'], [r'chosen', r'Bold project: Second', r'ERROR']),
	('startup-project-set', dict(MakeToolsCopy(), **{
		#Set as Startup Project: kept in the workspace file, and what GCC links of it is what Start runs, in its folder.
		'STARTUP_PROJECT': '0:Second', 'CONFIGURATION': 'Release|GCC', 'DEBUG': 'printstartup,printworkspace'}),
		[r'^Set startup project 0:Second: done$', r'^Startup project of Tools: \[Second\] of 2 applications \(chosen\)$', r'^Bold project: Second$',
		 r'^Start runs: .*Output[/\\]GCC[/\\]Release[/\\]Second(\.exe)? in .*Tools$', r'Tools\.sln" Startup="true" StartupProject="Second"/>'],
		[r'Bold project: First', r'ERROR']),
	('startup-project-library', dict(MakeToolsCopy(), **{
		#A library can't be a startup project.
		'STARTUP_PROJECT': '0:Shared', 'DEBUG': 'printstartup'}),
		[r'^Set startup project 0:Shared: refused$', r'^Startup project of Tools: \[First\] of 2 applications$'], [r'chosen', r'ERROR']),
	('startup-project-build', dict(MakeToolsCopy(), **{
		#An HCP65 build builds the startup project and the library, not the other application.
		'STARTUP_PROJECT': '0:Second', 'CONFIGURATION': 'Release|W65C816', 'DEBUG': 'command:Build.Build,wait:200,printbuild'}),
		[r'Second: Release\|Win32', r'Shared: Release\|Win32', r'build succeeded'], [r'First: Release\|Win32', r'ERROR']),
	('find-in-solution', {
		#Find in Files, whatever the case: the first results with where the text is in each, the first previewed and
		#opened with the text selected, and all of them in the Find tool window.
		'WAIT_INDEX': '1', 'FIND': '0:-:calculatorparser'},
		[r'^Find \[calculatorparser\]: 13 matches in 2 files', r'^  class CCalculatorParser  \[CalculatorParser\.h 29\]  match 7\+16$',
		 r'^  #include "CalculatorParser\.h"  \[CalculatorParser\.cpp 26\]  match 10\+16$', r'^Preview: CalculatorParser\.h line 29 of ',
		 r'^Opened CalculatorParser\.h 29:24$', r'^Find window: 13 results', r"^Occurrences of 'calculatorparser' in the solution  13 matches in 2 files$",
		 r'^  CalculatorParser\.h  1 match$', r'^    29: class CCalculatorParser$', r'^  CalculatorParser\.cpp  12 matches$'], [r'ERROR']),
	('find-match-case', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'FIND': '0:c:calculatorparser'},
		[r'^Find \[calculatorparser\]: Nothing found', r'^Find window: 0 results'], [r'^Opened ', r'ERROR']),
	('find-in-project-words', {
		#In the active file's project, whole words with their case: not CCalculatorParser's.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'FIND': '1:cw:Parse'},
		[r'^Find \[Parse\]: \d+\+? matches in \d+\+? files', r'^  CCalcObject\* CCalculatorParser::Parse\(const char\* szText\)  \[CalculatorParser\.cpp 57\]  match 32\+5$',
		 r"^Occurrences of 'Parse' in BaseLib  \d+ matches in \d+ files"], [r'\[CalculatorParser\.cpp 33\]', r'ERROR']),
	('find-in-directory', {
		#In a directory and under it, whatever is in a project.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'FIND': '2:-:class CCalculatorParser', 'FIND_DIRECTORY': os.path.join(ROOT, 'CommonLibraries', 'Libraries')},
		[r'^Find \[class CCalculatorParser\]: 1 match in 1 file', r'^  class CCalculatorParser  \[CalculatorParser\.h 29\]  match 0\+23$',
		 r"^Occurrences of 'class CCalculatorParser' in .*Libraries  1 match in 1 file$", r'^  CalculatorParser\.h  1 match$'], [r'ERROR']),
	('find-in-current-file', {
		#The Current File scope: only the active file is searched, though others have the text.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'FIND': '4:-:IsParentheses'},
		[r'^Find \[IsParentheses\]: 1 match in 1 file', r'^  bool  IsParentheses\(void\) const override;  \[CalcParentheses\.h 38\]  match 6\+13$',
		 r"^Occurrences of 'IsParentheses' in CalcParentheses\.h  1 match in 1 file$"], [r'Calculator\.cpp', r'ERROR']),
	('find-in-open-files', {
		#The Open Files scope: the files with an editor's tab.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'FIND': '5:-:IsParentheses'},
		[r'^Find \[IsParentheses\]: \d+ match(es)? in \d+ files?', r'\[CalcParentheses\.h 38\]', r"^Occurrences of 'IsParentheses' in the open files  "],
		[r'\[Calculator\.cpp ', r'ERROR']),
	('find-dialog-open-in-find-window', {
		#The dialog shown, and Open in Find Window clicked.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfind'},
		[r'^Dialog \[Find in Files\] answered Open in Find Window$', r"^Occurrences of 'calculatorparser' in the solution  13 matches in 2 files$",
		 r'^    29: class CCalculatorParser$'], [r'ERROR']),
	('close-cancel-and-navigate', {
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'X', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'closetab,answered:1,printtabs,edit:caret:26:12,gotosource,printtabs,switchfile,printtabs'},
		[r'^Tabs: .*>CalculatorParser\.cpp\*$', r'^Tabs: .*CalculatorParser\.cpp\* >CalculatorParser\.h$',
		 r'^Tabs: .*>CalculatorParser\.cpp\* CalculatorParser\.h$'], []),
	('exit-dont-save', {
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'EDIT': 'type:X,open:CalculatorSymbols.cpp,type:Y',
		'DIALOG_ANSWER': "Don't Save", 'DEBUG': 'exit', 'SNAPSHOT': None},
		[], []),
	('git-and-diffs', {
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'X', 'CHANGE': '36', 'DIALOG_ANSWER': "Don't Save",
		'DEBUG': 'wait:300,printgit,clickmenu:Git/Compare with HEAD,printdiffs,closeall,printdiffs'},
		[r'^Git: branch \[Git: \w+\] change popup open$',
		 r'^Diffs: \[>CalculatorParser\.cpp \(HEAD vs\. Your version\)\]$', r'^Diffs:$'], []),
	('git-change-popup-closes', {
		#The change popup shows the change as it was when it opened, so a press in the code, an edit, Escape and a
		#scroll each close it (Rollback would otherwise put the old lines over lines an edit had moved there), and it
		#is told however it closed.  It opens at the left of the code in view, though X typed in line 36 scrolls the
		#code sideways.  Its change bar is clicked for real each time.
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'X', 'DIALOG_ANSWER': "Don't Save",
		'DEBUG': 'wait:300,edit:changebarclick:36,printgit,edit:presseditor,printgit,'
		         'edit:changebarclick:36,printgit,edit:type:Y,printgit,'
		         'edit:changebarclick:36,printgit,edit:keys:Escape,printgit,'
		         'edit:changebarclick:36,printgit,edit:keys:Control+End,wait:300,printgit'},
		[r'^Git: branch \[Git: \w+\] change popup open$', r"^Git: change popup 0 right of the code's left$", r'^Pressed: the editor$', r'^Git: branch \[Git: \w+\] change popup closed$',
		 r'^Git: branch \[Git: \w+\] change popup open$', r'^Git: branch \[Git: \w+\] change popup closed$', r'^Git: branch \[Git: \w+\] change popup open$', r'^Git: branch \[Git: \w+\] change popup closed$', r'^Git: branch \[Git: \w+\] change popup open$', r'^Git: branch \[Git: \w+\] change popup closed$'], []),
	('git-dialogs', {
		'OPEN': 'CalculatorParser.cpp', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:Git/Compare with Revision...,answered:1,printdiffs,clickmenu:Git/Compare with Branch...,answered:2,printdiffs'},
		[r'^Diffs: \[>CalculatorParser\.cpp \(HEAD~1 vs\. Your version\)\]$',
		 r'^Diffs: \[CalculatorParser\.cpp \(HEAD~1 vs\. Your version\)\] \[>CalculatorParser\.cpp \(\w+ vs\. Your version\)\]$'], []),
	('debug-dialog', {
		'OPEN': 'Kernal.cpp', 'DIALOG_ANSWER': 'Cancel', 'DEBUG': 'clickmenu:Debug/New Breakpoint at Address...,answered:1,printtabs'},
		[r'^Tabs: .*>Kernal\.cpp'], [r'not found', r'disabled']),
	('commands', {
		'OPEN': 'Kernal.cpp', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'command:Debug.NewBreakpoint,answered:1,printtabs,command:Window.CloseAllDocuments,printtabs,command:No.Such'},
		[r'^Tabs: .*>Kernal\.cpp', r'^Tabs:\s*$', r'^Command \[No\.Such\] not found'], []),
	('keys-and-buttons', {
		'DEBUG': 'printkeys,printbuttons'},
		[r'^Keys: Navigate\.GoToDefinition F12 bound$',
		 r'^Keys: View\.IncreaseFontSize Control\+= bound$', r'^Keys: View\.DecreaseFontSize Control\+- bound$', r'^Keys: View\.ResetFontSize Control\+0 bound$', r'^Keys: Navigate\.GoToClass Control\+N bound$', r'^Keys: Navigate\.GoToAction Control\+Shift\+A bound$',
		 r'^Keys: Navigate\.GoToImplementation Control\+Alt\+B bound$', r'^Keys: Navigate\.GoToSuperMethod Control\+U bound$',
		 r'^Keys: Navigate\.GoToTypeDeclaration Control\+Shift\+B bound$', r'^Keys: Navigate\.GoToRelatedSymbol Control\+Alt\+Home bound$',
		 r'^Keys: Navigate\.GoToLine Control\+G bound$', r'^Keys: Navigate\.FileStructure Control\+F12 bound$', r'^Keys: Navigate\.NextMethod Alt\+Cursor Down bound$',
		 r'^Keys: Navigate\.PreviousMethod Alt\+Cursor Up bound$', r'^Keys: View\.Structure Alt\+7 bound$',
		 r'^Keys: Bookmarks\.Toggle Control\+F11 bound$', r'^Keys: Bookmarks\.ToggleMnemonic Control\+Shift\+F11 bound$', r'^Keys: Bookmarks\.Next F2 bound$',
		 r'^Keys: Bookmarks\.Previous Shift\+F2 bound$', r'^Keys: Bookmarks\.GoTo1 Control\+1 bound$', r'^Keys: Bookmarks\.GoTo9 Control\+9 bound$',
		 r'^Keys: Navigate\.NextChange Control\+Alt\+Shift\+Cursor Down bound$', r'^Keys: Navigate\.PreviousChange Control\+Alt\+Shift\+Cursor Up bound$',
		 r'^Keys: Navigate\.HighlightUsages Control\+Shift\+F7 bound$',
		 r'^Keys: Navigate\.RecentFiles Control\+E bound$', r'^Keys: Navigate\.RecentLocations Control\+Shift\+E bound$',
		 r'^Keys: Navigate\.LastEditLocation Control\+Shift\+Backspace bound$', r'^Keys: Navigate\.SelectIn Alt\+F1 bound$',
		 r'^Keys: Window\.Switcher Control\+Tab bound$', r'^Keys: Window\.SwitcherBackward Control\+Shift\+Tab bound$',
		 r'^Keys: Window\.NextTab Alt\+Cursor Right bound$', r'^Keys: Window\.PreviousTab Alt\+Cursor Left bound$',
		 r'^Keys: View\.QuickDefinition Control\+Shift\+I bound$', r'^Keys: View\.ParameterInfo Control\+P bound$', r'^Keys: View\.TypeInfo Control\+Shift\+P bound$',
		 r'^Keys: Navigate\.TypeHierarchy Control\+H bound$', r'^Keys: Navigate\.MethodHierarchy Control\+Shift\+H bound$', r'^Keys: Navigate\.CallHierarchy Control\+Alt\+H bound$',
		 r'^Keys: View\.Hierarchy Alt\+8 bound$', r'^Keys: View\.Bookmarks Alt\+2 bound$', r'^Keys: View\.FindUsages Alt\+3 bound$', r'^Keys: View\.Run Alt\+4 bound$',
		 r'^Keys: View\.Debug Alt\+5 bound$', r'^Keys: View\.Build Alt\+6 bound$', r'^Keys: View\.Find Alt\+9 bound$', r'^Keys: View\.HideAllToolWindows Control\+Shift\+F12 bound$',
		 r'^Keys: Navigate\.ShowUsages Control\+Alt\+F7 bound$', r'^Keys: Edit\.Find Control\+F bound$', r'^Keys: Edit\.Replace Control\+R bound$',
		 r'^Keys: Edit\.FindNext F3 bound$', r'^Keys: Edit\.FindPrevious Shift\+F3 bound$', r'^Keys: Edit\.FindWordAtCaret Control\+F3 bound$',
		 r'^Keys: Edit\.FindInFiles Control\+Shift\+F bound$', r'^Keys: Edit\.ReplaceInFiles Control\+Shift\+R bound$',
		 r'^Keys: Build\.Build Control\+F9 bound$', r'^Keys: Run\.Start Control\+F5 bound$', r'^Keys: Debug\.Start F5 bound$',
		 r'^Keys: Debug\.BreakAll Control\+Pause/Break bound$',
		 r'^Buttons: Start \[Run\.Start\]$'], [r'NOT BOUND', r'\[none\]']),
	('configuration', {
		'CONFIGURATION': 'Debug|W65C816', 'DEBUG': 'printconfig'},
		[r'^Configuration: \[Debug\|W65C816\] \[Debug\|W65C816\]: Debug\|W65C816, Release\|W65C816, Edit Configurations\.\.\.,$'], []),
	('system-directories-newest', {
		#The newest MinGW GCC by its number, not its name, and not a folder that isn't a version.
		'SYSTEM_ROOT': MakeSystemRoot(['9-win32', '13-win32', 'notes']), 'DEBUG': 'printsystem'},
		[r'^System directories: \S*/13-win32/include/c\+\+, \S*/13-win32/include/c\+\+/x86_64-w64-mingw32, \S*/usr/share/mingw-w64/include, \S*/13-win32/include,$'], [r'/9-win32', r'/notes']),
	('system-directories-threads', {
		#Of one version win32 threads, as the MSVC headers these stand in for have, but a newer version over them.
		'SYSTEM_ROOT': MakeSystemRoot(['13-posix', '13-win32']), 'DEBUG': 'printsystem'},
		[r'^System directories: \S*/13-win32/include/c\+\+,'], [r'/13-posix']),
	('system-directories-newer-threads', {
		'SYSTEM_ROOT': MakeSystemRoot(['13-win32', '14-posix']), 'DEBUG': 'printsystem'},
		[r'^System directories: \S*/14-posix/include/c\+\+,'], [r'/13-win32']),
	('system-directories-found', {
		#Nothing is said when they are all there.
		'SYSTEM_ROOT': MakeSystemRoot(['13-win32']), 'DEBUG': 'wait:100'},
		[], [r"MinGW's headers weren't found"]),
	('system-directories-none', {
		#The status line says why code navigation finds nothing.
		'SYSTEM_ROOT': MakeSystemRoot([], False), 'DEBUG': 'printsystem'},
		[r"^Status: MinGW's headers weren't found, so code navigation won't work: ", r'^System directories:$'], []),
	('system-directories-no-library', {
		#The compiler's without the C library's are just as bad.
		'SYSTEM_ROOT': MakeSystemRoot(['13-win32'], False), 'DEBUG': 'printsystem'},
		[r"^Status: MinGW's headers weren't found", r'^System directories: \S*/13-win32/include/c\+\+,'], []),
	('build-configuration-rules', dict(OpenedForCheck([os.path.join(KERNAL, 'HCP65Kernal.sln')]), **{
		#OK is refused for what the file couldn't have, and the real file isn't written by a scripted run.  The kernal's
		#configurations have an image, and a card with a path on it (the boot loader's \OS\OS.SYS): a card given with
		#the path cleared is refused, then one with a path is fine.
		'DIALOG_ANSWER': 'OK',
		'BUILD_EDITS': 'name:Release|W65C816;check;name:;check;name:Fine;solution:Debug|Win32;card:Card.img;cardpath:;check;cardpath:\\\\boot.exe;toolchain:GCC;check;toolchain:W65C816;check',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printconfig'}),
		[r'^Build dialog: refused \[Another build configuration has this name\.\]$',
		 r'^Build dialog: refused \[A build configuration needs a name\.\]$',
		 r'^Build dialog: refused \[A card and a path on it go together\.\]$',
		 r'^Build dialog: refused \[Only an HCP65 toolchain can link a program, load files, use a card or build for its debugger\.\]$',
		 r'^Build dialog: OK \[\]$', r'^Dialog \[Build Configurations: HCP65Kernal\] answered OK$',
		 r"^Status: A scripted run doesn't save build configurations\.$",
		 r'^Configuration: \[Debug\|W65C816\] \[Debug\|W65C816\]:'], []),
	('build-configuration-save', dict(MakeSolutionCopy(), **{
		#What is changed goes in the file; its comment stays; the run configuration follows the rename, and the copy gets one.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'name:Renamed|W65C816;breakpoint:Kernal;nofloats:1;add;name:Copied|W65C816',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printconfig,printbuildfile'}),
		[r'^Configuration: \[.*\] \[Renamed\|W65C816\]: .*Copied\|W65C816, Edit Configurations\.\.\.,$',
		 r'^<!-- A comment to keep\. -->$',
		 r'<Configuration Name="Renamed\|W65C816" Toolchain="W65C816" .*BreakpointSpace="Kernal" NoFloats="true"',
		 r'<Configuration Name="Copied\|W65C816" Toolchain="W65C816"'], [r'ERROR']),
	('build-configuration-override-warning', dict(MakeSolutionCopy(), **{
		#The override warning is on unless unticked, which is all that is written.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'overridewarning:0',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printbuildfile'}),
		[r'<Configuration Name="[^"]*" Toolchain="W65C816" [^>]*WarnMissingOverride="false"'], [r'ERROR', r'WarnMissingOverride="true"']),
	('build-configuration-exceptions', dict(MakeSolutionCopy(), **{
		#C++ exceptions are off unless ticked, which is all that is written.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'exceptions:1',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printbuildfile'}),
		[r'<Configuration Name="[^"]*" Toolchain="W65C816" [^>]*Exceptions="true"'], [r'ERROR', r'Exceptions="false"']),
	('build-configuration-stub-missing', dict(MakeSolutionCopy(), **{
		#A missing function is a build error unless ticked, which is all that is written.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'stubmissing:1',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printbuildfile'}),
		[r'<Configuration Name="[^"]*" Toolchain="W65C816" [^>]*StubMissing="true"'], [r'ERROR', r'StubMissing="false"']),
	('build-configuration-debug', dict(MakeSolutionCopy(), **{
		#Built for the debugger, and projects compiled without the optimiser: only an HCP65 toolchain's, and written when set.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'debug:1;unoptimised:Init, Shell;toolchain:GCC;check;toolchain:W65C816;check',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printbuildfile'}),
		[r'^Build dialog: refused \[Only an HCP65 toolchain can link a program, load files, use a card or build for its debugger\.\]$', r'^Build dialog: OK \[\]$',
		 r'<Configuration Name="[^"]*" Toolchain="W65C816" [^>]*Debug="true" Unoptimised="Init, Shell"'], [r'ERROR']),
	('build-configuration-cancel', dict(MakeSolutionCopy(), **{
		#Cancel changes nothing.
		'DIALOG_ANSWER': 'Cancel', 'BUILD_EDITS': 'name:Gone;remove',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printbuildfile'}),
		[r'^<!-- A comment to keep\. -->$', r'<Configuration Name="Release\|W65C816"'], [r'Gone']),
	('build-configuration-open-file', dict(MakeSolutionCopy(), **{
		#Open File opens the file instead of saving.
		'DIALOG_ANSWER': 'Open File', 'BUILD_EDITS': 'name:Gone',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printtabs,printbuildfile'}),
		[r'^Tabs: .*>Kernel\.hcp65build', r'<Configuration Name="Release\|W65C816"'], [r'Gone']),
	('build-configuration-swap', dict(MakeSolutionCopy(True), **{
		#Swapping two names: each run configuration follows what it built, not the name another has just been given, and
		#the old names still being in the solution saved isn't another solution having them.
		'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Fast;build:Release|W65C816;add;name:Small;build:Other|W65C816',
		'BUILD_EDITS': 'select:0;name:Other|W65C816;select:1;name:Release|W65C816',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Build.EditConfigurations,answered:2,printstatus,printconfig,printrunconfigurations,printbuildfile'}),
		[r'^Status line: Saved the build configurations\.$',
		 r'^Configuration: \[Small\] \[Release\|W65C816\]:',
		 r'^Run configuration: Fast builds Other\|W65C816$',
		 r'^Run configuration: Small builds Release\|W65C816$',
		 r'<Configuration Name="Other\|W65C816" Toolchain="W65C816" Solution="Release\|Win32" BreakpointSpace="Kernel">',
		 r'<Configuration Name="Release\|W65C816" Toolchain="W65C816" Solution="Release\|Win32"/>'], [r'ERROR']),
	('build-configuration-remove', dict(MakeSolutionCopy(True), **{
		#The run configuration that was only for the one removed goes with it, and one that is left is chosen.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'remove',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printstatus,printconfig,printbuildfile'}),
		[r'^Status line: Saved the build configurations\.$',
		 r'^Configuration: \[Other\|W65C816\] \[Other\|W65C816\]: Other\|W65C816, Edit Configurations\.\.\.,$',
		 r'<Configuration Name="Other\|W65C816"'], [r'Name="Release\|W65C816"', r'ERROR']),
	('build-configuration-remove-kept', dict(MakeSolutionCopy(True), **{
		#One with a tool is kept and stays chosen, as it says.  It builds nothing, as Build says, but its tool still runs.
		'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Emu;tool:/bin/sh;arguments:-c true', 'BUILD_EDITS': 'remove',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Build.EditConfigurations,answered:2,printstatus,printconfig,command:Build.Build,wait:200,printbuild,command:Run.Start,wait:500,printrun'}),
		[r'^Status line: Saved, but run configuration Emu builds a build configuration you removed: choose another for it in Edit Configurations\.$',
		 r'^Configuration: \[Emu\] \[\]: Other\|W65C816, Emu, Edit Configurations\.\.\.,$',
		 r'^Emu builds Release\|W65C816, which no open solution has: choose another build configuration in Edit Configurations\.$',
		 r'^Running: /bin/sh -c true$', r'^Exited with code 0\.$'], [r'ERROR']),
	('build-configuration-remove-last', dict(MakeSolutionCopy(), **{
		#With none left there is nothing to choose, and Build says so.
		'DIALOG_ANSWER': 'OK', 'BUILD_EDITS': 'remove',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printconfig,command:Build.Build,wait:200,printbuild'}),
		[r'^Configuration: \[.*\] \[\]: Edit Configurations\.\.\.,$',
		 r'^No run configuration is chosen: add one in Edit Configurations\.$'], [r'ERROR']),
	('build-nothing-to-build', {
		#However it came to be, a run configuration whose build configuration isn't there says so when built.
		'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Nothing;build:No|Such',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Build.Build,wait:200,printbuild'},
		[r'^Nothing builds No\|Such, which no open solution has: choose another build configuration in Edit Configurations\.$'], []),
	('run-configuration-tool', {
		#A run configuration made in the dialog is chosen with its build configuration, and Start runs its tool with its
		#arguments and working directory, without the debugger, and says how it exited.
		'DIALOG_ANSWER': 'OK',
		'RUN_EDITS': 'add;name:Echo;build:Release|W65C816;tool:/bin/sh;directory:/tmp;arguments:-c pwd',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,printconfig,command:Run.Start,wait:500,printrun'},
		[r'^Dialog \[Run Configurations\] answered OK$',
		 r'^Configuration: \[Echo\] \[Release\|W65C816\]: Debug\|W65C816, Release\|W65C816, Echo, Edit Configurations\.\.\.,$',
		 r'^Run: not running \[Start\]$', r'^Running: /bin/sh -c pwd$', r'^/tmp$', r'^Exited with code 0\.$'], []),
	('run-configuration-stop', {
		#Start again while it runs stops it, and the button says Stop until then.
		'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Sleep;tool:/bin/sleep;arguments:30',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Run.Start,wait:300,printrun,command:Run.Stop,wait:300,printrun'},
		[r'^Run: running \[Stop\]$', r'^Running: /bin/sleep 30$', r'^Run: not running \[Start\]$', r'^Exited with code '], []),
	('run-configuration-default-tool', {
		#Without a tool a W65C816 one starts the default emulator, with its own folder and arguments.
		'EMULATOR': '/bin/sh', 'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Default;directory:/tmp;arguments:-c pwd',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Run.Start,wait:500,printrun'},
		[r'^Running: /bin/sh -c pwd$', r'^/tmp$', r'^Exited with code 0\.$'], []),
	('run-configuration-nothing-to-start', {
		#Without a tool, and no build configuration to say it is a W65C816 one, it says what to do.
		'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Nothing;build:No|Such',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Run.Start,wait:200,printrun'},
		[r'^Run: not running \[Start\]$', r'^No tool is set for Nothing: give it one in Edit Configurations\.$'], []),
	('run-configuration-rules', {
		#OK is refused for a name used twice or none; Cancel changes nothing, and removing them all gives the defaults.
		#Which of them is chosen is whatever the user's ModPlay workspace last chose, so either is.
		'DIALOG_ANSWER': 'Cancel',
		'RUN_EDITS': 'add;name:Debug|W65C816;check;name:;check;name:Fine;check;remove;remove;remove;check',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,printconfig'},
		[r'^Run dialog: refused \[Another run configuration has this name\.\]$',
		 r'^Run dialog: refused \[A run configuration needs a name\.\]$',
		 r'^Run dialog: OK \[\]$', r'^Run dialog: OK \[\]$',
		 r'^Configuration: \[(Debug|Release)\|W65C816\] \[\1\|W65C816\]: Debug\|W65C816, Release\|W65C816, Edit Configurations\.\.\.,$'], []),
	('run-configuration-no-tool', {
		#A tool that isn't there says so when it starts.
		'DIALOG_ANSWER': 'OK', 'RUN_EDITS': 'add;name:Missing;tool:/no/such/tool',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,command:Run.Start,wait:500,printrun'},
		[r'^Run: not running \[Start\]$', r'^Running: /no/such/tool$'], []),
	('explorer', {
		'OPEN': 'CalculatorParser.cpp', 'EDIT': 'explorer', 'DEBUG': 'printexplorer'},
		[r'^Explorer: \[.*[/\\]BaseLib[/\\]CalculatorParser\.cpp\]$'], []),
	('breakpoint', {
		'OPEN': 'Kernal.cpp', 'DEBUG': 'line:20,buildtab,wait:300,printtabs'},
		[r'^Tabs: .*>Kernal\.cpp'], []),
] + DebuggingChecks(DEBUGGING_W65C816) + DebuggingChecks(DEBUGGING_65C64) + [
	('menus', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DEBUG': 'printmenus,clickmenu:View/Nope'},
		[r'^Menu File: New>, Save File as Template\.\.\., Open Project/Solution\.\.\., Add Solution\.\.\., Close Solution, Recent Solutions>, Clear Recent Solutions, Save \[Ctrl\+S\], Save All \[Ctrl\+Shift\+S\], Compare with File\.\.\., Settings\.\.\. \[Ctrl\+Alt\+S\], Exit$',
		 r'^Menu File/New: C\+\+ Class\.\.\., C\+\+ Header\.\.\., C\+\+ Source\.\.\.$',
		 r'^Menu File/Recent Solutions: 1 ',
		 r'^Menu Edit: Undo \[Ctrl\+Z\](\(disabled\))?, Redo \[Ctrl\+Shift\+Z\](\(disabled\))?, Cut \[Ctrl\+X\], Copy \[Ctrl\+C\], Paste \[Ctrl\+V\], Paste from History\.\.\. \[Ctrl\+Shift\+V\], Select All \[Ctrl\+A\], Extend Selection \[Ctrl\+W\], Shrink Selection \[Ctrl\+Shift\+W\], Find\.\.\. \[Ctrl\+F\], Replace\.\.\. \[Ctrl\+R\], Find Next \[F3\], Find Previous \[Shift\+F3\], Find Word at Caret \[Ctrl\+F3\], Find in Files\.\.\. \[Ctrl\+Shift\+F\], Replace in Files\.\.\. \[Ctrl\+Shift\+R\], Find Duplicates\.\.\. \[Ctrl\+Alt\+Shift\+D\]$',
		 r'^Menu Navigate: Back \[Ctrl\+Alt\+Left\](\(disabled\))?, Forward \[Ctrl\+Alt\+Right\](\(disabled\))?, Class\.\.\. \[Ctrl\+N\], File\.\.\. \[Ctrl\+Shift\+N\], Symbol\.\.\. \[Ctrl\+Alt\+Shift\+N\], Action\.\.\. \[Ctrl\+Shift\+A\], Line:Column\.\.\. \[Ctrl\+G\], Go to Source \[Ctrl\+B\], Go to Definition \[F12\], Go to Implementation\(s\) \[Ctrl\+Alt\+B\], Go to Type Declaration \[Ctrl\+Shift\+B\], Go to Super Method \[Ctrl\+U\], Go to Related Symbol \[Ctrl\+Alt\+Home\], Switch Header/Source \[Alt\+O\], File Structure \[Ctrl\+F12\], Next Method \[Alt\+Down\], Previous Method \[Alt\+Up\], Toggle Bookmark \[Ctrl\+F11\], Toggle Bookmark with Mnemonic\.\.\. \[Ctrl\+Shift\+F11\], Next Bookmark \[F2\], Previous Bookmark \[Shift\+F2\], Next Change \[Ctrl\+Alt\+Shift\+Down\], Previous Change \[Ctrl\+Alt\+Shift\+Up\], Highlight Usages in File \[Ctrl\+Shift\+F7\], Recent Files \[Ctrl\+E\], Recent Locations \[Ctrl\+Shift\+E\], Last Edit Location \[Ctrl\+Shift\+Backspace\], Next Edit Location, Select In\.\.\. \[Alt\+F1\], Type Hierarchy \[Ctrl\+H\], Method Hierarchy \[Ctrl\+Shift\+H\], Call Hierarchy \[Ctrl\+Alt\+H\], Find Usages \[Alt\+F7\], Show Usages \[Ctrl\+Alt\+F7\], Find All References \[Shift\+F12\], Find Usages Settings\.\.\. \[Ctrl\+Alt\+Shift\+F7\], Next Occurrence \[Ctrl\+Alt\+Down\], Previous Occurrence \[Ctrl\+Alt\+Up\]$',
		 r'^Menu View: Solution Explorer \[Alt\+1\], Structure \[Alt\+7\], Find Usages \[Alt\+3\], Find \[Alt\+9\], Duplicates, Bookmarks \[Alt\+2\], Hierarchy \[Alt\+8\], Build \[Alt\+6\], Indexing, Problems, Run \[Alt\+4\], Debug \[Alt\+5\], Locals, Call Stack, Autos, Watch 1, Watch 2, Watch 3, Watch 4, Claude, Hide All Tool Windows \[Ctrl\+Shift\+F12\], Jump to Last Tool Window(\(disabled\))?, Quick Definition \[Ctrl\+Shift\+I\], Parameter Info \[Ctrl\+P\], Type Info \[Ctrl\+Shift\+P\], Increase Font Size \[Ctrl\+=\], Decrease Font Size \[Ctrl\+-\], Reset Font Size \[Ctrl\+0\]$',
		 r'^Menu Git: Compare with HEAD, Compare with Branch\.\.\., Compare with Revision\.\.\.$',
		 r'^Menu Build: Build \[Ctrl\+F9\], Edit Build Configurations\.\.\.$',
		 r'^Menu Debug: Start Debugging \[F5\], Start Without Debugging \[Ctrl\+F5\], Stop Debugging \[Shift\+F5\], Stop Running(\(disabled\))?, Restart \[Ctrl\+Shift\+F5\], Break All \[Ctrl\+Pause/Break\], Attach to Emulator\.\.\., Step Into \[F11\], Step Over \[F10\], Step Out \[Shift\+F11\], Toggle Breakpoint \[F9\], New Breakpoint at Address\.\.\., New Data Breakpoint\.\.\.(\(disabled\))?, Delete All Breakpoints, Edit Configurations\.\.\., Windows>, QuickWatch\.\.\. \[Shift\+F9\](\(disabled\))?$',
		 r'^Menu Window: Reset Window Layout, Close All Documents, Switcher \[Ctrl\+Tab\], Select Next Tab \[Alt\+Right\], Select Previous Tab \[Alt\+Left\], Reopen Closed Tab(\(disabled\))?$',
		 r'^Menu Editor: Go to Source \[Ctrl\+B\], Find Usages \[Alt\+F7\], Find Usages Settings\.\.\. \[Ctrl\+Alt\+Shift\+F7\], Show Context Actions \[Alt\+Enter\], Rename\.\.\. \[Shift\+F6\], Refactor>, Add Watch, QuickWatch\.\.\. \[Shift\+F9\](\(disabled\))?, Cut \[Ctrl\+X\], Copy \[Ctrl\+C\], Paste \[Ctrl\+V\], Paste from History\.\.\. \[Ctrl\+Shift\+V\]$',
		 r"^Menu Editor/Refactor: Rename File\.\.\., Safe Delete\.\.\. \[Alt\+Delete\], Introduce Variable\.\.\. \[Ctrl\+Alt\+V\], Introduce Field\.\.\. \[Ctrl\+Alt\+F\], Introduce Constant\.\.\. \[Ctrl\+Alt\+C\], Introduce Parameter\.\.\. \[Ctrl\+Alt\+P\], Introduce Typedef\.\.\. \[Ctrl\+Alt\+K\](\(disabled\))?, Inline\.\.\. \[Ctrl\+Alt\+N\], Change Signature\.\.\. \[Ctrl\+F6\], Unify Parameter Names, Generate Definition \[Ctrl\+Shift\+D\], Generate Declaration, Move Definition out of Class, Move Definition into Class, Pull Members Up\.\.\., Push Members Down\.\.\., Move\.\.\. \[F6\], Copy Class\.\.\., Move to Namespace\.\.\., Extract Method\.\.\. \[Ctrl\+Alt\+M\](\(disabled\))?, Extract Superclass\.\.\., Extract Interface\.\.\., Extract Class\.\.\., Encapsulate Field\.\.\., Optimise \#includes \[Ctrl\+Alt\+O\], Convert Macro\.\.\., Convert Pointer to Reference, Convert Reference to Pointer, Convert Enum to Enum Class\.\.\., Write Template's Interface\.\.\., Make Member Function const, Make Member Function static, Convert Function to Member Function, Convert Member Function to Function, Extract Template Parameter\.\.\., Remove Template Parameter$",
		 r'^Menu Explorer: Set as Startup Solution, Set as Startup Project(\(disabled\))?, Remove Solution, Rename\.\.\., Properties\.\.\.(\(disabled\))?, Always Select Opened File, Open Files with Single Click$',
		 r'^Menu New: C\+\+ Class\.\.\., C\+\+ Header\.\.\., C\+\+ Source\.\.\., Copy Class\.\.\.$',
		 r'^Menu item \[View/Nope\] not found$'], []),
	('menu-clicks', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'clickmenu:Editor/Rename...,answered:1,clickmenu:Navigate/File...,answered:2,clickmenu:Editor/Find Usages,'
			'clickmenu:Editor/Go to Source,printtabs,clickmenu:Edit/Select All,clickmenu:View/Build,'
			'clickmenu:Window/Reset Window Layout,clickmenu:Explorer/Set as Startup Solution,clickmenu:File/Save All,printtabs'},
		[r'^Dialog \[Rename\] answered Cancel$', r'^Dialog \[Go To\] answered Cancel$',
		 r'^Tabs: .*>CalculatorParser\.h$', r'^Tabs: .*>CalculatorParser\.h$'], [r'not found', r'disabled']),
	('hierarchy-after-edits', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'renamable,replace:29| : public CCalcExpression|,renamable'},
		[r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\) const\] at 7 places, 1 overrides$',
		 r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\) const\] at 2 places, 0 overrides$'], []),
	('virtual-after-edits', {
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'CalcExpression.h', 'CARET': '37:18',
		'EDIT': 'renamable,replace:37|virtual |,caret:37:9,renamable'},
		[r'^Rename: IsParentheses \[CCalcExpression::IsParentheses\(\) const\] at 7 places, 1 overrides$',
		 r'^Rename: IsParentheses \[CCalcExpression::IsParentheses\(\) const\] at 5 places, 0 overrides$'], []),
	('settings-templates', dict(MakeDefinitionsCopy(), **{
		#Settings' File and Code Templates: the Workspace scheme (a solution is open) and its Files templates, C++ Class
		#first with its child under it; the Code tab's; a template changed (marked) with an error shown under it, then reset; an added one
		#renamed and removed; a built-in one can't be removed.  Cancel keeps nothing.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Cancel',
		'TEMPLATE_EDITS': 'show;tab:Code;show;select:New Function Body;text:#if ($BODY)\nx\n;show;reset;show;add;name:Mine;show;remove;show;select:Getter Body;remove;show',
		'DEBUG': 'clickmenu:File/Settings...,answered:1'}),
		[r'^Templates row: C\+\+ Class$', r'^Templates row:   C\+\+ Class Source$', r'^Templates row: C\+\+ Header$', r'^Templates row: C\+\+ Source$',
		 r'^Templates selected: C\+\+ Class, error \[\], status \[\]$',
		 r'^Templates row: Definition Banner$', r'^Templates row: New Function Body \[changed\]$',
		 r'^Templates selected: New Function Body, error \[Line 1, column 1: #if has no #end\], status \[\]$',
		 r'^Templates selected: New Function Body, error \[\], status \[\]$', r'^Templates row: Mine \[changed\]$', r'^Templates selected: Mine, error \[\], status \[\]$',
		 r'^Templates selected: Getter Body, error \[\], status \[A built-in template can.t be removed: Reset gives it back as it was\.\]$',
		 r'^Dialog \[Settings\] answered Cancel$'],
		[r'^Templates: kept']),
	('settings-templates-keep', dict(MakeDefinitionsCopy(), **{
		#OK keeps the Workspace scheme's change: its fileTemplates beside the solution have it, and Generate Definition
		#writes it.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'OK', 'OPEN': 'Tank.h', 'CARET': '14:8',
		'TEMPLATE_EDITS': 'tab:Code;select:New Function Body;text:\t//Kept.\n',
		'DEBUG': 'clickmenu:File/Settings...,answered:1,edit:renamesearch:---s,edit:generatedefinition,printdisklines:Tank.cpp,printdisklines:fileTemplates/Templates.xml'}),
		[r'^Dialog \[Settings\] answered OK$', r'^Templates: kept$', r'^Status: Defined CTank::Fill in Tank\.cpp\.$', r'^29\|\{$', r'^30\|\t//Kept\.$', r'^31\|\}$',
		 r'^\d+\|\s*<Template Tab="Code" Name="New Function Body"/>$'],
		[]),
	('settings-list', {
		#Every command, grouped and filtered; the keys are as they were and nothing is kept.
		'DIALOG_ANSWER': 'Cancel', 'KEYMAP_EDITS': 'select:Edit.Copy;show;filter:delete;show',
		'DEBUG': 'clickmenu:File/Settings...,answered:1,printkeys,printkeymap'},
		[r'^Keymap row: File\.OpenSolution \[\]$', r'^Keymap row: File\.Exit \[\]$', r'^Keymap row: Edit\.Copy \[Ctrl\+C, Ctrl\+Insert\]$',
		 r'^Keymap row: Git\.CompareWithHead \[\]$', r'^Keymap row: Debug\.StepOver \[F10\]$', r'^Keymap row: Window\.CloseDocument \[Ctrl\+F4\]$',
		 r'^Keymap row: Explorer\.RemoveSolution \[\]$', r'^Keymap row: Editor\.Up \[Up\]$', r'^Keymap row: Editor\.ToggleOverwrite \[Insert\]$',
		 r'^Keymap row: Edit\.Cut \[Ctrl\+X, Shift\+Delete\]$', r'^Keymap row: Editor\.DeleteLine \[Ctrl\+Y\]$',
		 r'^Dialog \[Settings\] answered Cancel$', r'^Keys: Edit\.Copy Control\+C bound$', r'^Keys: Edit\.Copy Control\+Insert bound$',
		 r'^Keys: Editor\.Up Cursor Up bound$', r'^Keys: Window\.CloseDocument Control\+F4 bound$', r'^Keymap file:$', r'^\(none\)$'],
		[r'NOT BOUND', r'ERROR']),
	('settings-tab', {
		#Tab goes round the dialog from the filter: the list, its buttons, OK and Cancel, its pages and back to the filter.
		'DIALOG_ANSWER': 'Cancel', 'DIALOG_KEYS': 'tab,tab,tab,tab,tab,tab,tab,tab,tab,backtab,backtab', 'DEBUG': 'command:File.Settings,answered:1'},
		[r'^Focus: CListContainer$', r'^Focus: CButton$', r'^Focus: CButton$', r'^Focus: CButton$', r'^Focus: CButton$', r'^Focus: CButton$', r'^Focus: CButton$',
		 r'^Focus: CListContainer$', r'^Focus: CTextField$', r'^Focus: CListContainer$', r'^Focus: CButton$'], [r'ERROR']),
	('keymap-save', {
		#Keys added and taken away apply straight away and are kept, and a command's others stay.
		'DIALOG_ANSWER': 'OK', 'KEYMAP': MakeKeymapFile(),
		'KEYMAP_EDITS': 'select:Navigate.GoToSource;add:Control+K;select:Debug.StepOver;remove;select:Edit.Copy;remove;add:Control+Shift+K',
		'DEBUG': 'command:File.Settings,answered:1,printstatus,printkeys,printmenus,printkeymap'},
		[r'^Keymap: Ctrl\+K is free\.$', r'^Keymap: Ctrl\+Shift\+K is free\.$', r'^Status line: Saved the keymap\.$',
		 r'^Keys: Edit\.Copy Control\+Shift\+K bound$', r'^Keys: Navigate\.GoToSource Control\+B bound$', r'^Keys: Navigate\.GoToSource Control\+K bound$',
		 r'Copy \[Ctrl\+Shift\+K\]', r'Step Over,', r'^Menu Editor: Go to Source \[Ctrl\+B\]',
		 r'^Keymap file:$', r'Name="Edit\.Copy"', r'Control\+Shift\+K', r'Name="Navigate\.GoToSource"', r'Control\+B', r'Control\+K', r'Name="Debug\.StepOver"'],
		[r'Keys: Debug\.StepOver', r'Keys: Edit\.Copy Control\+C', r'Name="File\.Save"', r'NOT BOUND', r'ERROR']),
	('keymap-read', {
		#Those kept come back as the IDE starts, and a command that isn't one any more is ignored.
		'KEYMAP': MakeKeymapFile('<Keymap>\n  <Command Name="Navigate.GoToSource"><Keys>Control+K</Keys></Command>\n'
			'  <Command Name="Debug.StepOver"/>\n  <Command Name="Edit.Copy"><Keys>Control+Shift+K</Keys></Command>\n'
			'  <Command Name="No.Such"><Keys>F1</Keys></Command>\n</Keymap>\n'),
		'DEBUG': 'printkeys,printmenus'},
		[r'^Keys: Edit\.Copy Control\+Shift\+K bound$', r'^Keys: Navigate\.GoToSource Control\+K bound$', r'Copy \[Ctrl\+Shift\+K\]', r'^Menu Editor: Go to Source \[Ctrl\+K\]'],
		[r'Keys: Debug\.StepOver', r'Keys: Navigate\.GoToSource Control\+B', r'Keys: Edit\.Copy Control\+C', r'No\.Such', r'NOT BOUND', r'ERROR']),
	('keymap-bad-file', {
		#One that isn't a keymap is said, and the default keys are used.
		'KEYMAP': MakeKeymapFile('<NotAKeymap/>\n'), 'DEBUG': 'printkeys'},
		[r"^Status: The keymap couldn't be read: the default keys are used\.$", r'^Keys: Edit\.Copy Control\+C bound$', r'^Keys: Debug\.StepOver F10 bound$'], [r'NOT BOUND']),
	('keymap-cancel', {
		#Nothing changes, nothing is written.
		'DIALOG_ANSWER': 'Cancel', 'KEYMAP': MakeKeymapFile(), 'KEYMAP_EDITS': 'select:Debug.StepOver;remove;add:Control+K',
		'DEBUG': 'command:File.Settings,answered:1,printkeys,printkeymap'},
		[r'^Keys: Debug\.StepOver F10 bound$', r'^\(none\)$'], [r'Keys: Debug\.StepOver Control\+K', r'Saved the keymap']),
	('keymap-move', {
		#A key has one command, so giving it to another moves it, and two can swap theirs.
		'DIALOG_ANSWER': 'OK', 'KEYMAP': MakeKeymapFile(),
		'KEYMAP_EDITS': 'select:Navigate.GoToSource;add:F5;select:Debug.Start;add:Control+B;select:Navigate.GoToSource;show',
		'DEBUG': 'command:File.Settings,answered:1,printkeys'},
		[r'^Keymap: F5 moves here from Debug: Start Debugging\.$', r'^Keymap: Ctrl\+B moves here from Navigate: Go to Source\.$',
		 r'^Keymap row: Navigate\.GoToSource \[F5\]$', r'^Keymap row: Debug\.Start \[Ctrl\+B\]$',
		 r'^Keys: Navigate\.GoToSource F5 bound$', r'^Keys: Debug\.Start Control\+B bound$'],
		[r'Keys: Navigate\.GoToSource Control\+B', r'Keys: Debug\.Start F5', r'NOT BOUND', r'ERROR']),
	('keymap-refused', {
		#What types can't be had by anything, and what a dialog or text field needs only by an editor's.  A Global key an
		#editor action has stays the editor's there, which is said.
		'DIALOG_ANSWER': 'OK', 'KEYMAP': MakeKeymapFile(),
		'KEYMAP_EDITS': 'select:Debug.StepOver;add:Q;add:Shift+Q;add:Space;add:Enter;add:Control+C;add:Cursor Up;add:F10;add:Control+D;select:Editor.Up;add:Q;add:F8;select:Edit.Copy;add:Tab',
		'DEBUG': 'command:File.Settings,answered:1,printkeys'},
		[r'^Keymap: Q types a character: add Ctrl or Alt\.$', r'^Keymap: Shift\+Q types a character: add Ctrl or Alt\.$', r'^Keymap: Space types a character: add Ctrl or Alt\.$',
		 r'^Keymap: Enter is needed by dialogs and text fields\.$', r'^Keymap: Ctrl\+C moves here|^Keymap: Ctrl\+C is needed by dialogs and text fields\.$', r'^Keymap: Up is needed by dialogs and text fields\.$',
		 r'^Keymap: F10 is already its shortcut\.$', r'^Keymap: Ctrl\+D is free\.  Editor Actions: Duplicate keeps it over the Editor\.$',
		 r'^Keymap: Q types a character: add Ctrl or Alt\.$', r'^Keymap: F8 is free\.$', r'^Keymap: Tab moves here from Editor Actions: Tab\.$',
		 r'^Keys: Edit\.Copy Tab bound$', r'^Keys: Editor\.Up Cursor Up bound$', r'^Keys: Editor\.Up F8 bound$', r'^Keys: Editor\.Duplicate Control\+D bound$', r'^Keys: Debug\.StepOver F10 bound$', r'^Keys: Debug\.StepOver Control\+D bound$'],
		[r'Keys: Debug\.StepOver Q', r'Keys: Debug\.StepOver Enter', r'Keys: Editor\.Tab', r'NOT BOUND', r'ERROR']),
	('keymap-reset', {
		#One command back, taking its keys from whichever has them, then all of them, which leaves nothing to keep.
		'DIALOG_ANSWER': 'OK', 'KEYMAP': MakeKeymapFile(),
		'KEYMAP_EDITS': 'select:Navigate.GoToSource;add:F5;select:Debug.Start;reset;select:Navigate.GoToSource;show;select:Debug.StepOver;remove;resetall',
		'DEBUG': 'command:File.Settings,answered:1,printkeys,printkeymap'},
		[r'^Keymap row: Navigate\.GoToSource \[Ctrl\+B\]$', r'^Keys: Debug\.Start F5 bound$', r'^Keys: Debug\.StepOver F10 bound$', r'^Keymap file:$', r'<Keymap'],
		[r'Keys: Navigate\.GoToSource F5', r'Name="', r'NOT BOUND', r'ERROR']),
	('keymap-capture', {
		#Add Shortcut, over the Settings dialog: OK gives the command the keys.
		'DIALOG_ANSWER': 'OK', 'KEYMAP': MakeKeymapFile(), 'KEYMAP_EDITS': 'select:Navigate.GoToSource;capture:Control+K',
		'DEBUG': 'command:File.Settings,answered:2,printkeys'},
		[r'^Dialog \[Add Shortcut\] answered OK$', r'^Dialog \[Settings\] answered OK$', r'^Keys: Navigate\.GoToSource Control\+K bound$'], [r'ERROR']),
	('keymap-capture-cancel', {
		#Cancel in Add Shortcut, and Settings is still there to be answered.
		'DIALOG_ANSWER': 'Cancel', 'KEYMAP': MakeKeymapFile(), 'KEYMAP_EDITS': 'select:Navigate.GoToSource;capture:Control+K',
		'DEBUG': 'command:File.Settings,answered:2,printkeys'},
		[r'^Dialog \[Add Shortcut\] answered Cancel$', r'^Dialog \[Settings\] answered Cancel$'], [r'Keys: Navigate\.GoToSource Control\+K', r'ERROR']),
	('file-commands', {
		#Menu items that became commands run from their names, as their keys would.  A file chooser isn't answered, so
		#those aren't run here.
		'OPEN': 'CalculatorParser.cpp', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'command:Git.CompareWithBranch,answered:1,command:Git.CompareWithRevision,answered:2,command:Git.CompareWithHead,printdiffs,command:Window.ResetLayout,printtabs'},
		[r'^Dialog \[.*\] answered Cancel$', r'^Dialog \[.*\] answered Cancel$', r'^Diffs: \[>CalculatorParser\.cpp \(HEAD vs\. Your version\)\]$', r'^Tabs: .*CalculatorParser\.cpp'], [r'not found']),
	('view-menu', {
		'OPEN': 'CalculatorParser.cpp', 'DEBUG': 'clickmenu:View/Debug,clickmenu:Window/Close All Documents,printtabs'},
		[r'^Tabs:$'], [r'not found', r'disabled']),
	('editor-clicks', {
		#A double click in the editor selects the word under the pointer, a triple click its line with its line feed (the
		#caret then on the next line).  Real presses through the window, column 10 being the P of IsParentheses.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1',
		'DEBUG': 'wait:1500,edit:doubleclick:38:10,edit:printselection,wait:1000,edit:tripleclick:38:10,edit:printselection'},
		[r'^Selection: \[IsParentheses\] on line 38, caret line 38$', r'^Selection: \[\tbool\t\tIsParentheses\(void\) const override;$', r'^\] on line 39, caret line 39$'], []),
	('editor-drag-text', {
		#Dragging the selected text moves it: IsParentheses, selected by a double click, is pressed on and dropped at
		#the start of line 39's "void", where it stays selected.  Real presses through the window.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1',
		'DEBUG': 'wait:1500,edit:doubleclick:38:10,wait:1000,edit:dragtext:38:12:39:2,edit:printselection,edit:print'},
		[r'^Selection: \[IsParentheses\] on line 39, caret line 39$', r'CalcParentheses\.h \* \(focus\) line 39: \[\tIsParenthesesvoid\t\tPrint\(CChars\* psz\) override;\]$'], []),
	('editor-occurrences', {
		#The selected text's other occurrences in the file are highlighted: void, selected by a double click on line
		#38, is in the header six more times (case and all).  They are found at a tick after the selection changes.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1',
		'DEBUG': 'wait:1500,edit:doubleclick:38:22,wait:500,edit:printselection,edit:printoccurrences'},
		[r'^Selection: \[void\] on line 38, caret line 38$', r'^Occurrences: 6 at 34:2 35:2 35:13 36:20 37:2 39:2$'], []),
	('editor-caret-usages', {
		#The caret resting on a symbol (300 ms) highlights its usages in the file, where it is declared marked w: pcChild in
		#Calculator.cpp's StripParentheses.  Moved within one they stay; moved off, or the file edited, they go at once;
		#after an edit they come back once the index has the edited text (where the edit moved one, there).  Only this
		#file's: StripParentheses' definition and uses, not its declaration in Calculator.h.
		'RECENT': CALCULATOR_RECENT, 'WAIT_INDEX': '1', 'OPEN': 'Calculator.cpp', 'CARET': '363:21',
		'DEBUG': 'wait:1000,edit:printcaretusages,edit:caret:369:5,edit:printcaretusages,edit:caret:365:3,edit:printcaretusages,'
		         'edit:caret:363:21,wait:50,edit:printcaretusages,wait:1000,edit:printcaretusages,edit:replace:370|= pcChild|=  pcChild,edit:printcaretusages,'
		         'wait:3000,edit:printcaretusages,edit:caret:361:20,wait:1000,edit:printcaretusages'},
		[r'^Caret usages: 4 at 363:20w 369:4 370:21 371:13$', r'^Caret usages: 4 at 363:20w 369:4 370:21 371:13$', r'^Caret usages: 0 at$', r'^Caret usages: 0 at$',
		 r'^Caret usages: 4 at 363:20w 369:4 370:21 371:13$', r'^Caret usages: 0 at$', r'^Caret usages: 4 at 363:20w 369:4 370:22 371:13$',
		 r'^Caret usages: 6 at 361:19w 394:3 400:3 401:3 408:3 423:3$'], []),
	('editor-extend-selection', {
		#Extend Selection grows through the syntax as well as the text: from mpcLeft to the member access (the syntax's),
		#the brackets' inside then the brackets (the text's), the call (the syntax's); Shrink goes back.  Straight after an
		#edit (before the edits pause) the text is parsed for it then and there.
		'OPEN': 'Calculator.cpp', 'CARET': '400:33',
		'DEBUG': 'wait:2000,edit:extend,edit:printselection,edit:extend,edit:printselection,edit:extend,edit:printselection,'
		         'edit:extend,edit:printselection,edit:extend,edit:printselection,edit:shrink,edit:printselection,'
		         'edit:replace:401|->mpcRight)|->mpcRight ),edit:caret:401:33,edit:extend,edit:extend,edit:printselection'},
		[r'^Selection: \[mpcLeft\] on line 400', r'^Selection: \[pcBinary->mpcLeft\] on line 400', r'^Selection: \[&pcBinary->mpcLeft\] on line 400',
		 r'^Selection: \[\(&pcBinary->mpcLeft\)\] on line 400', r'^Selection: \[StripParentheses\(&pcBinary->mpcLeft\)\] on line 400',
		 r'^Selection: \[\(&pcBinary->mpcLeft\)\] on line 400', r'^Selection: \[pcBinary->mpcRight\] on line 401'], []),
	('editor-box-selection', {
		#Alt+Shift and the arrows make a box (column) selection, as Visual Studio's: from void on line 37 down two rows and
		#right four characters is each row's first word.  Typing replaces each, leaving a caret on each row; undo puts the
		#text back and ends the box.
		'OPEN': 'CalcParentheses.h', 'CARET': '37:2',
		'DEBUG': 'wait:1500,edit:keys:Alt+Shift+Cursor Down,edit:keys:Alt+Shift+Cursor Down,edit:keys:Alt+Shift+Cursor Right,'
		         'edit:keys:Alt+Shift+Cursor Right,edit:keys:Alt+Shift+Cursor Right,edit:keys:Alt+Shift+Cursor Right,edit:printbox,'
		         'edit:key:type:X,edit:printbox,edit:print,command:Edit.Undo,edit:printbox,edit:print'},
		[r'^Box: \[void\] \[bool\] \[void\]$', r'^Box: \[\] \[\] \[\]$', r'CalcParentheses\.h \* \(focus\) line 39: \[\tX\t\tPrint\(CChars\* psz\) override;\]$',
		 r'^Box: none$', r'CalcParentheses\.h \(focus\) line 39: \[\tvoid\t\tPrint\(CChars\* psz\) override;\]$'], []),
	('editor-gutter-select', {
		#Pressing a line's number in the gutter selects the line with its line feed; a shift press on line 36's number
		#then selects from where that selection started (line 38's start) up to line 36's start.  Real presses through
		#the window.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1',
		'DEBUG': 'wait:1500,edit:gutterclick:38,edit:printselection,wait:1000,edit:shiftgutterclick:36,edit:printselection'},
		[r'^Selection: \[\tbool\t\tIsParentheses\(void\) const override;$', r'^\] on line 39, caret line 39$', r'^Selection: \[\tCNumber\t\tEvaluate\(void\) override;$', r'^\tvoid\t\tSetExpression\(CCalcExpression\* pcExpression\);$', r'^\] on line 38, caret line 36$'], []),	('editor-paste-history', {
		#Paste from History (Control+Shift+V), as IntelliJ IDEA's: three words copied from line 38 are the clipboard's
		#history, newest first; Choose Content to Paste lists them, down selects and previews the second, and OK pastes
		#it at the caret and makes it the newest, so a plain Control+V pastes it again.  The run's clipboard is its own,
		#not the system's.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1', 'DIALOG_KEYS': 'down', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'wait:1500,edit:doubleclick:38:2,edit:keys:Control+C,edit:doubleclick:38:10,edit:keys:Control+C,edit:doubleclick:38:22,edit:keys:Control+C,edit:printclipboardhistory,edit:caret:1:1,edit:keys:Control+Shift+V,answered:1,edit:printclipboardhistory,edit:keys:Control+V,edit:print'},
		[r'^Clipboard history: 3 \[void\] \[IsParentheses\] \[bool\]$', r'^Paste history \[\]: void \*IsParentheses bool$', r'^Paste preview: \[IsParentheses\]$',
		 r'^Dialog \[Choose Content to Paste\] answered OK$', r'^Clipboard history: 3 \[IsParentheses\] \[void\] \[bool\]$',
		 r'^Edit menu:.* Paste Paste from History\.\.\. Select All', r'CalcParentheses\.h \* \(focus\) line 1: \[IsParenthesesIsParentheses/\*\* -'], []),
	('editor-paste-block', {
		#What a box selection copies is a block: pasted at a plain caret it goes in as a column, a row to each line from
		#the caret down.  "bool" and "void" from lines 38 and 39, pasted at the start of line 1: line 2, which is empty,
		#gets the second.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:2',
		'DEBUG': 'wait:1500,edit:keys:Alt+Shift+Cursor Down,edit:keys:Alt+Shift+Cursor Right,edit:keys:Alt+Shift+Cursor Right,edit:keys:Alt+Shift+Cursor Right,edit:keys:Alt+Shift+Cursor Right,edit:printbox,edit:keys:Control+C,edit:printclipboardhistory,edit:caret:1:1,edit:keys:Control+V,edit:print'},
		[r'^Box: \[bool\] \[void\]$', r'^Clipboard history: 1 \[bool$', r'^void\]$', r'CalcParentheses\.h \* \(focus\) line 2: \[void\]$'], []),
	('editor-document-keys-scroll', {
		#Control+End and Control+Home, pressed on a keyboard (the script's, which the component input hears as it hears
		#a real one), move the caret to the document's end and start and the view with it.  The keys' actions keep the
		#layout, so the scroll pane has to ask for the layout that moves the text itself: it didn't, and the view stayed
		#where it was unless something else happened to lay the window out.
		'OPEN': 'CalculatorParser.cpp',
		'DEBUG': 'wait:1500,edit:printview,edit:press:Left Control,edit:tap:End,edit:release:Left Control,wait:300,edit:printview,edit:press:Left Control,edit:tap:Home,edit:release:Left Control,wait:300,edit:printview'},
		[r'^View: lines 1 to \d+, caret line 1$', r'^View: lines \d+ to 420, caret line 420$', r'^View: lines 1 to \d+, caret line 1$'], []),
	('nav-implementations-chooser', {
		#Go to Implementation (Control+Alt+B) on a virtual function with several overrides: a chooser under the caret lists
		#them, the nearest classes' first, each with where it is and a preview of its code, the first selected; down and
		#enter go to the second, its name selected.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '14:17',
		'DEBUG': 'command:Navigate.GoToImplementation,edit:printchooser,edit:key:down,edit:printchooser,edit:key:enter,edit:printchooser,edit:printselection,edit:print'},
		[r'^Chooser: Implementations of CShape::Draw  3$', r'^  >CCircle::Draw \| Shapes\.cpp:4$', r'^   CSquare::Draw \| Shapes\.cpp:16$',
		 r'^   CRoundedSquare::Draw \| Shapes\.cpp:27$', r'^Chooser filter: \[\]$', r'^Chooser preview: Shapes\.cpp$',
		 r'^  >CSquare::Draw \| Shapes\.cpp:16$', r'^Chooser: none$', r'^Selection: \[Draw\] on line 16',
		 r'Shapes\.cpp \(focus\) line 16: \[void CSquare::Draw\(void\)\]'], []),
	('nav-implementations-filter', {
		#Typing in the chooser filters its rows as Go to matches; escape closes it and the editor has the keys again, the
		#caret where it was.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '14:17',
		'DEBUG': 'command:Navigate.GoToImplementation,edit:key:type:squ,edit:printchooser,edit:key:escape,edit:printchooser,edit:print'},
		[r'^Chooser: Implementations of CShape::Draw  3$', r'^  >CSquare::Draw \| Shapes\.cpp:16$', r'^   CRoundedSquare::Draw \| Shapes\.cpp:27$',
		 r'^Chooser filter: \[squ\]$', r'^Chooser: none$', r'Shape\.h \(focus\) line 14: '], [r'CCircle::Draw \|']),
	('nav-implementation-one', {
		#With one override it is gone to without a chooser.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '23:8',
		'DEBUG': 'command:Navigate.GoToImplementation,edit:printchooser,edit:print'},
		[r'^Chooser: none$', r'Shapes\.cpp \(focus\) line 27: \[void CRoundedSquare::Draw\(void\)\]'], []),
	('nav-implementation-definition', {
		#From the declaration of a function nothing overrides, its definition.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '16:10',
		'DEBUG': 'command:Navigate.GoToImplementation,edit:print'},
		[r'Shape\.cpp \(focus\) line 16: \[void CShape::Move\(int iX, int iY\)\]'], []),
	('nav-implementations-class', {
		#From a class, the classes derived from it, however far down, the nearest first.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '6:8',
		'DEBUG': 'command:Navigate.GoToImplementation,edit:printchooser,edit:key:escape'},
		[r'^Chooser: Implementations of CShape  3$', r'^  >CCircle \| Shapes\.h:6$', r'^   CSquare \| Shapes\.h:17$', r'^   CRoundedSquare \| Shapes\.h:28$'], []),
	('nav-implementations-none', {
		#A class nothing derives from, and a function defined where the caret is: the status line says there are none.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '28:8',
		'DEBUG': 'command:Navigate.GoToImplementation,printstatus,edit:open:Shapes.cpp,edit:caret:27:22,command:Navigate.GoToImplementation,printstatus'},
		[r'^Status: No implementations of CRoundedSquare\.$', r'^Status: No implementations of CRoundedSquare::Draw\.$'], []),
	('nav-super-methods', {
		#Go to Super Method (Control+U) from inside a function's body, where no symbol is named: what the function overrides
		#in its class's base classes, the nearest class's first.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '28:1',
		'DEBUG': 'command:Navigate.GoToSuperMethod,edit:printchooser,edit:key:enter,edit:print'},
		[r'^Chooser: Super methods of CRoundedSquare::Draw  2$', r'^  >CSquare::Draw \| Shapes\.cpp:16$', r'^   CShape::Draw \| Shape\.cpp:11$',
		 r'Shapes\.cpp \(focus\) line 16: \[void CSquare::Draw\(void\)\]'], []),
	('nav-super-method-one', {
		#One super method is gone to: a pure virtual function's declaration, as it has no definition.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '10:14',
		'DEBUG': 'command:Navigate.GoToSuperMethod,edit:print'},
		[r'Shape\.h \(focus\) line 15: \[\tvirtual int\t\tArea\(void\) = 0;\]'], []),
	('nav-super-class', {
		#From a class, its base class; from a function that overrides nothing, the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '28:8',
		'DEBUG': 'command:Navigate.GoToSuperMethod,edit:print,edit:open:Shape.h,edit:caret:14:17,command:Navigate.GoToSuperMethod,printstatus'},
		[r'Shapes\.h \(focus\) line 17: \[class CSquare : public CShape\]', r'^Status: No super methods of CShape::Draw\.$'], []),
	('nav-related-symbol', {
		#Go to Related Symbol (Control+Alt+Home): everything the caret has a way to, under headings: the file's header,
		#the function's other declaration, what it overrides and what overrides it.  Down and Enter go to the second.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '16:15',
		'DEBUG': 'command:Navigate.GoToRelatedSymbol,edit:printchooser,edit:key:down,edit:key:enter,edit:print'},
		[r'^Chooser: CSquare::Draw$', r'^   \[Header\]$', r'^  >Shapes\.h \| $', r'^   \[Declarations and definition\]$', r'^   CSquare::Draw \| Shapes\.h:23$',
		 r'^   \[Super methods\]$', r'^   CShape::Draw \| Shape\.cpp:11$', r'^   \[Implementations\]$', r'^   CRoundedSquare::Draw \| Shapes\.cpp:27$',
		 r'Shapes\.h \(focus\) line 23: \[\tvoid\tDraw\(void\) override;\]'], []),
	('nav-related-none', {
		#Where nothing is related (a file with no other half, no symbol at the caret) the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '1:1',
		'DEBUG': 'command:Navigate.GoToRelatedSymbol,edit:printchooser,printstatus'},
		[r'^Status: Nothing is related to what is at the caret\.$', r'^Chooser: none$'], []),
	('nav-show-usages', {
		#Show Usages (Control+Alt+F7) on a use of a name, not only its declaration: the chooser of its usages, each row
		#its line with where it is, the preview showing its file; typing filters by the lines' text and Enter goes to the
		#one left, its name selected.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '12:12',
		'DEBUG': 'command:Navigate.ShowUsages,edit:printchooser,edit:key:type:circ,edit:printchooser,edit:key:enter,edit:printselection,edit:print'},
		[r'^Chooser: Usages of Draw and 3 overrides  4 usages$', r'^  >pcShape->Draw\(\); \| Main\.cpp:12$', r'^   pcCircle->Draw\(\); \| Main\.cpp:13$',
		 r'^   Draw\(\); \| Shape\.cpp:20$', r'^   CSquare::Draw\(\); \| Shapes\.cpp:29$', r'^Chooser preview: Main\.cpp$',
		 r'^  >pcCircle->Draw\(\); \| Main\.cpp:13$', r'^Chooser filter: \[circ\]$', r'^Selection: \[Draw\] on line 13',
		 r'Main\.cpp \(focus\) line 13: '], []),
	('nav-control-click', {
		#Control+click on a name goes where Go to Source would from it: from a use of CShape's Draw to its definition;
		#from that, its own definition, the chooser of its usages; and off any name, nowhere, which the status line says.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '1:1',
		'DEBUG': 'edit:controlclick:12:12,edit:print,edit:controlclick:11:15,edit:printchooser,edit:key:escape,edit:controlclick:12:1,printstatus,edit:printselection'},
		[r'Shape\.cpp \(focus\) line 11: \[void CShape::Draw\(void\)\]', r'^Chooser: Usages of Draw and 3 overrides  4 usages$',
		 r'^Status: No definition found at the caret\.$', r'^Selection: none'], []),
	('nav-control-click-include', {
		#Control+click on an #include's file opens it.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '2:1',
		'DEBUG': 'edit:controlclick:1:13,edit:print'},
		[r'Shapes\.h \(focus\) line 1: '], []),
	('nav-outline', {
		#A file's outline: what it declares, in order, each class's members under it, a function's parameters and
		#qualifiers after its name, a definition marked, with its name's line and all of its range.  A line typed is in
		#it at once: the file is parsed there and then when the outline is asked for.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '16:1',
		'DEBUG': 'edit:printoutline,edit:type:int Extra(CShape* pcShape);,edit:printoutline'},
		[r'^Outline: 10 entries$', r'^class\* CCircle 6 \[6:1-14:2\]$', r'^  field miRadius 9 \[9:2-9:16\]$', r'^  function Draw\(void\) override 12 \[12:2-12:27\]$',
		 r'^class\* CSquare 17 \[17:1-25:2\]$', r'^class\* CRoundedSquare 28 \[28:1-32:2\]$', r'^  function Draw\(void\) override 31 ',
		 r'^Outline: 11 entries$', r'^function Extra\(CShape\* pcShape\) 16 \[16:1-16:28\]$', r'^class\* CSquare 17 '], []),
	('nav-outline-definitions', {
		#Functions defined outside their classes are named as they are written, and marked as definitions with their
		#bodies' ranges; nothing declared in a body is in the outline.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '1:1',
		'DEBUG': 'edit:printoutline,edit:open:Shapes.cpp,edit:printoutline'},
		[r'^Outline: 3 entries$', r'^function\* TotalArea\(CShape\* pcFirst, CShape\* pcSecond\) 4 \[4:1-7:2\]$', r'^function\* main\(void\) 17 \[17:1-26:2\]$',
		 r'^Outline: 5 entries$', r'^function\* CCircle::Draw\(void\) 4 \[4:1-7:2\]$', r'^function\* CRoundedSquare::Draw\(void\) 27 \[27:1-30:2\]$'],
		[r'cCircle;', r'cSquare;', r'^variable ']),
	('nav-file-structure', {
		#File Structure (Control+F12): a chooser of the file's outline, each entry indented under the one it is in, with
		#its line; the entry the caret is in is selected.  Typing filters by name; Down and Enter go to the second left.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '20:5',
		'DEBUG': 'command:Navigate.FileStructure,edit:printchooser,edit:key:type:dra,edit:printchooser,edit:key:down,edit:key:enter,edit:printchooser,edit:print'},
		[r'^Chooser: Structure of Shapes\.h$', r'^   CCircle \| 6$', r'^       Draw\(void\) override \| 12$', r'^   CSquare \| 17$', r'^  >    miSide \| 20$',
		 r'^   CRoundedSquare \| 28$', r'^Chooser filter: \[\]$', r'^  >    Draw\(void\) override \| 12$', r'^       Draw\(void\) override \| 23$',
		 r'^       Draw\(void\) override \| 31$', r'^Chooser filter: \[dra\]$', r'^Chooser: none$', r'Shapes\.h \(focus\) line 23: '], [r'miSide \| 20\n.*\[dra\]']),
	('nav-next-method', {
		#Next Method (Alt+Down) and Previous Method (Alt+Up) put the caret on the next function's name after the caret, or
		#the last before it; with none that way the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'command:Navigate.NextMethod,edit:print,command:Navigate.NextMethod,edit:print,command:Navigate.PreviousMethod,command:Navigate.PreviousMethod,'
		         'command:Navigate.PreviousMethod,edit:print,command:Navigate.PreviousMethod,printstatus'},
		[r'Shapes\.cpp \(focus\) line 16: \[void CSquare::Draw\(void\)\]', r'Shapes\.cpp \(focus\) line 21: \[int CSquare::Area\(void\)\]',
		 r'Shapes\.cpp \(focus\) line 4: \[void CCircle::Draw\(void\)\]', r'^Status: There is no method before the caret\.$'], []),
	('nav-braces', {
		#Move Caret to Matching Brace (Control+Shift+M): from before a function's opening brace to its closing one and
		#back.  Move Caret to Code Block Start and End (Control+[ and Control+]): from inside the body to before its
		#opening brace, and to after its closing one; with Shift the text between is selected.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '5:1',
		'DEBUG': 'edit:keys:Control+Shift+M,edit:print,edit:keys:Control+Shift+M,edit:print,edit:caret:6:3,edit:keys:Control+[,edit:print,'
		         'edit:caret:6:3,edit:keys:Control+],edit:print,edit:caret:6:3,edit:keys:Control+Shift+],edit:printselection'},
		[r'Shapes\.cpp.* line 7: \[\}\]', r'Shapes\.cpp.* line 5: \[\{\]', r'Shapes\.cpp.* line 5: \[\{\]', r'Shapes\.cpp.* line 7: \[\}\]',
		 r'^Selection: \[ove\(0, 0\);$', r'^\}\] on line 7, caret line 7$'], []),
	('nav-go-to-line', {
		#Go to Line (Control+G): a prompt over the editor with the caret's line and column in it.  What isn't a line is
		#said to be wrong and the prompt stays; a line and a column put the caret there and close it; a line past the
		#last is the last (the empty one after the last line feed), and Escape leaves the caret alone.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '4:6',
		'DEBUG': 'command:Navigate.GoToLine,edit:printprompt,edit:key:type:x,edit:key:enter,edit:printprompt,edit:key:backspace,edit:key:type:16:6,edit:key:enter,'
		         'edit:printprompt,edit:print,command:Navigate.GoToLine,edit:printprompt,edit:key:type:999,edit:key:enter,edit:print,'
		         'command:Navigate.GoToLine,edit:key:type:2,edit:key:escape,edit:printprompt,edit:print'},
		[r'^Prompt: Line\[:column\]: \[4:6\] $', r'^Prompt: Line\[:column\]: \[x\] Type a line, or a line and a column: 120 or 120:8\.$', r'^Prompt: none$',
		 r'Shapes\.cpp \(focus\) line 16: \[void CSquare::Draw\(void\)\]', r'^Prompt: Line\[:column\]: \[16:6\] $', r'Shapes\.cpp \(focus\) line 31: \[\]',
		 r'^Prompt: none$', r'Shapes\.cpp \(focus\) line 31: \[\]'], []),
	('nav-breadcrumbs', {
		#Breadcrumbs: under the code, what the caret is in, outermost first (a namespace, a class in it, a function
		#defined in the class), following the caret: only the namespace after the class's closing brace, a function
		#outside both, nothing between them.  A press on a crumb puts the caret on its name, and opens a chooser of what
		#is beside it when anything is: the class has nothing beside it, the function has the class's other members.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Long.cpp', 'CARET': '20:5',
		'DEBUG': 'edit:printcrumbs,edit:caret:100:5,edit:printcrumbs,edit:caret:124:3,edit:printcrumbs,edit:caret:127:1,edit:printcrumbs,'
		         'edit:caret:130:2,edit:printcrumbs,edit:caret:20:5,edit:crumbclick:1,edit:print,edit:printcrumbs,edit:printchooser,'
		         'edit:caret:20:5,edit:crumbclick:2,edit:printchooser,edit:key:down,edit:key:enter,edit:print,edit:printcrumbs'},
		[r'^Breadcrumbs: Deep > CLadder > Climb$', r'^Breadcrumbs: Deep > CLadder > Descend$', r'^Breadcrumbs: Deep$', r'^Breadcrumbs: \(none\)$',
		 r'^Breadcrumbs: Outside$', r'Long\.cpp \(focus\) line 4: \[class CLadder\]', r'^Breadcrumbs: Deep > CLadder$', r'^Chooser: none$',
		 r'^Chooser: In CLadder$', r'^   miRungs \| 7$', r'^  >Climb\(int iSteps\) \| 9$', r'^   Descend\(int iSteps\) \| 77$',
		 r'Long\.cpp \(focus\) line 77: ', r'^Breadcrumbs: Deep > CLadder > Descend$'], []),
	('nav-sticky-lines', {
		#Sticky lines: none while the view is at the top; scrolled into the class, the namespace's and the class's
		#first lines are kept over the code; into a function, its first line too, and the next function's once that
		#is what the top of the view is in.  A press on a line puts the caret on what it starts.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Long.cpp', 'CARET': '1:1',
		'DEBUG': 'edit:printsticky,edit:scroll:4,edit:printsticky,edit:scroll:30,edit:printsticky,edit:scroll:80,edit:printsticky,'
		         'edit:stickyclick:2,edit:print'},
		[r'^Sticky lines: \(none\)$', r'^Sticky lines: \[namespace Deep\] \[class CLadder\]$',
		 r'^Sticky lines: \[namespace Deep\] \[class CLadder\] \[\tint Climb\(int iSteps\)\]$',
		 r'^Sticky lines: \[namespace Deep\] \[class CLadder\] \[\tint Descend\(int iSteps\)\]$',
		 r'Long\.cpp \(focus\) line 77: \[\tint Descend\(int iSteps\)\]'], []),
	('nav-editor-settings', {
		#Settings' Editor page: breadcrumbs and sticky lines are shown until each is unticked and kept by OK, which
		#takes them off the open editors and is written with the settings.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Long.cpp', 'CARET': '40:5',
		'SETTINGS': MakeSettingsFile(), 'EDITOR_EDITS': 'breadcrumbs:false;sticky:false', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'edit:scroll:30,edit:printcrumbs,edit:printsticky,clickmenu:File/Settings...,answered:1,printsettings,edit:printcrumbs,edit:printsticky'},
		[r'^Breadcrumbs: Deep > CLadder > Climb$', r'^Sticky lines: \[namespace Deep\] \[class CLadder\] \[\tint Climb\(int iSteps\)\]$',
		 r'^Dialog \[Settings\] answered OK$', r'<Editor Breadcrumbs="false" StickyLines="false"/>', r'^Breadcrumbs: off$', r'^Sticky lines: off$'], []),
	('nav-editor-settings-read', {
		#The settings as they were written: no breadcrumbs, sticky lines.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Long.cpp', 'CARET': '40:5',
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <Editor Breadcrumbs="false" StickyLines="true"/>\n</Settings>\n'),
		'DEBUG': 'edit:scroll:30,edit:printcrumbs,edit:printsticky'},
		[r'^Breadcrumbs: off$', r'^Sticky lines: \[namespace Deep\] \[class CLadder\] \[\tint Climb\(int iSteps\)\]$'], []),
	('key-repeat-default', {
		#Held keys repeat in the IDE (CComponentInput::SetRepeat) as Windows' defaults do: after half a second, thirty
		#times a second.
		'SOLUTION': NAVIGATION, 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'printkeyrepeat'},
		[r'^Key repeat: 500 ms, every 33 ms$'], []),
	('key-repeat-settings', {
		#Settings' Editor page: the delay and rate typed are kept by OK, given to the window's input straight away, and
		#written with the settings.
		'SOLUTION': NAVIGATION, 'SETTINGS': MakeSettingsFile(), 'EDITOR_EDITS': 'repeat:true;delay:300;rate:20', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/Settings...,answered:1,printkeyrepeat,printsettings'},
		[r'^Dialog \[Settings\] answered OK$', r'^Key repeat: 300 ms, every 50 ms$', r'<KeyRepeat On="true" Delay="300" Rate="20"/>'], []),
	('key-repeat-off', {
		#Unticked, held keys don't repeat; a number out of range keeps what there was.
		'SOLUTION': NAVIGATION, 'SETTINGS': MakeSettingsFile(), 'EDITOR_EDITS': 'repeat:false;delay:5;rate:500', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/Settings...,answered:1,printkeyrepeat,printsettings'},
		[r'^Key repeat: off$', r'<KeyRepeat On="false" Delay="500" Rate="30"/>'], []),
	('key-repeat-read', {
		#The settings as they were written.
		'SOLUTION': NAVIGATION,
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <KeyRepeat On="true" Delay="250" Rate="40"/>\n</Settings>\n'),
		'DEBUG': 'printkeyrepeat'},
		[r'^Key repeat: 250 ms, every 25 ms$'], []),
	('font-zoom-keys', {
		#View's Increase and Decrease Font Size (Control+= and Control+-) zoom every code view a pixel at a time, the
		#active editor's lines with it; Reset Font Size (Control+0) goes back to 13.  Each is shown and written.
		'SOLUTION': NAVIGATION, 'OPEN': 'Main.cpp', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'printfontsize,command:View.IncreaseFontSize,command:View.IncreaseFontSize,printfontsize,command:View.DecreaseFontSize,printfontsize,'
		         'printsettings,command:View.ResetFontSize,printfontsize'},
		[r'^Font size: 13, editor lines 18 pixels, wheel zoom on$', r'^Status: Font size: 14$', r'^Status: Font size: 15$',
		 r'^Font size: 15, editor lines 21 pixels, wheel zoom on$', r'^Status: Font size: 14$', r'^Font size: 14, editor lines \d+ pixels',
		 r'<CodeFont Size="14" WheelZoom="true"/>', r'^Status: Font size: 13$', r'^Font size: 13, editor lines \d+ pixels'], []),
	('font-zoom-settings', {
		#Settings' Editor page: the size typed and the wheel's zoom unticked are kept by OK and written.
		'SOLUTION': NAVIGATION, 'OPEN': 'Main.cpp', 'SETTINGS': MakeSettingsFile(), 'EDITOR_EDITS': 'fontsize:18;wheelzoom:false', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/Settings...,answered:1,printfontsize,printsettings'},
		[r'^Dialog \[Settings\] answered OK$', r'^Font size: 18, editor lines \d+ pixels, wheel zoom off$', r'<CodeFont Size="18" WheelZoom="false"/>'], []),
	('font-zoom-range', {
		#A size out of range typed keeps what there was; zoomed, the size stops at 8 and 32.
		'SOLUTION': NAVIGATION, 'OPEN': 'Main.cpp', 'SETTINGS': MakeSettingsFile(), 'EDITOR_EDITS': 'fontsize:99', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'clickmenu:File/Settings...,answered:1,printfontsize,' + ','.join(['command:View.DecreaseFontSize'] * 7) + ',printfontsize'},
		[r'^Font size: 13, ', r'^Font size: 8, '], [r'^Font size: 7']),
	('font-zoom-read', {
		#The settings as they were written: the code opens in that size.
		'SOLUTION': NAVIGATION, 'OPEN': 'Main.cpp',
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <CodeFont Size="20" WheelZoom="false"/>\n</Settings>\n'),
		'DEBUG': 'printfontsize'},
		[r'^Font size: 20, editor lines \d+ pixels, wheel zoom off$'], []),
	('nav-structure-window', {
		#The Structure tool window: the active file's outline as a tree, the entry the caret is in selected as the
		#caret moves.  A click on a row puts the caret on its entry.  Its menu's Hide Fields leaves fields out (a caret
		#in one selects its class) and Sort by Name orders each node's entries; both are written with the settings.
		#View lists it after Solution Explorer, which it is tabbed with.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '20:5', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'edit:printstructure,edit:caret:12:10,edit:printstructure,edit:structureclick:5,edit:print,edit:structure:fields,edit:printstructure,'
		         'edit:structure:sort,edit:printstructure,printsettings,printmenus'},
		[r'^Structure: 10 rows$', r'^  CCircle$', r'^    miRadius$', r'^    Draw\(void\) override$', r'^  CSquare$', r'^>   miSide$', r'^  CRoundedSquare$',
		 r'^Structure: 10 rows$', r'^>   Draw\(void\) override$', r'^  CSquare$', r'Shapes\.h \(focus\) line 20: \[\tint\t\tmiSide;\]',
		 r'^Structure: 8 rows, no fields$', r'^  CCircle$', r'^    Draw\(void\) override$', r'^> CSquare$',
		 r'^Structure: 8 rows, by name, no fields$', r'^  CCircle$', r'^    Area\(void\) override$', r'^    Draw\(void\) override$', r'^  CRoundedSquare$', r'^> CSquare$',
		 r'<Structure SortByName="true" ShowFields="false"/>', r'^Menu View: Solution Explorer \[Alt\+1\], Structure \[Alt\+7\], Find Usages \[Alt\+3\], '], []),
	('nav-structure-settings-read', {
		#The Structure tree's choices as they were written.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '20:5',
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <Structure SortByName="true" ShowFields="true"/>\n</Settings>\n'),
		'DEBUG': 'edit:printstructure'},
		[r'^Structure: 10 rows, by name$', r'^  CCircle$', r'^    Area\(void\) override$', r'^    Draw\(void\) override$', r'^    miRadius$', r'^  CRoundedSquare$', r'^>   miSide$'], []),
	('nav-type-to-find', {
		#Typing finds a row.  Structure (Alt+7) has the keys: "are" selects the next entry from the selected one with
		#it, CSquare's Area, and Enter puts the caret there.  In Solution Explorer "sha" selects the first file from the
		#selected one with it, Down the next round the end, and Escape ends the find and leaves that one selected, for
		#Enter to open.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '20:5',
		'DEBUG': 'command:View.Structure,edit:printstructure,edit:key:type:are,edit:printstructure,edit:key:enter,edit:print,'
		         'edit:focus:explorer,edit:key:type:sha,printexplorer,edit:key:down,printexplorer,edit:key:escape,edit:key:enter,edit:print'},
		[r'^>   miSide$', r'^Structure: 10 rows$', r'^  CSquare$', r'^    Draw\(void\) override$', r'^>   Area\(void\) override$', r'^  CRoundedSquare$',
		 r'Shapes\.h \(focus\) line 24: \[\tint\t\tArea\(void\) override;\]', r'^Explorer: \[.*Navigation/Shapes\.h\]$', r'^Explorer: \[.*Navigation/Shape\.cpp\]$',
		 r'Shape\.cpp \(focus\) line 1: '], []),
	('nav-type-to-find-usages', {
		#In the Find Usages tree too: characters typed find a row, where once they did nothing.  Escape ends the find
		#and the keys stay in the tree; Escape again goes back to the editor.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shape.h', 'CARET': '14:17',
		'EDIT': 'usagegroups:,usages,focus:usages,printselected,key:type:squ,printselected,key:escape,print,key:escape,print'},
		[r'^Usages of Draw and 3 overrides  4 usages$', r'^Selected: 12: pcShape->Draw\(\);$', r'^Selected: 29: CSquare::Draw\(\);$',
		 r'Shape\.h line 14: \[\tvirtual void\tDraw\(void\);\]$', r'Shape\.h \(focus\) line 14: '], []),
	('find-bar', {
		#Find (Control+F): a bar over the code, its field the word at the caret.  Every match is counted and marked on
		#the scroll bar (a mark for each line with one), the first from the caret selected.  Enter goes to the next and
		#Shift+Enter to the previous, round the ends.  What is typed is searched for as it is typed; Match Case, Words
		#and Regex change what matches, and are written with the settings; what isn't a regular expression is said to
		#be wrong.  Escape closes the bar and leaves the match selected, the editor with the keys.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '10:16', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'edit:printfindbar,command:Edit.Find,edit:printfindbar,edit:printselection,edit:key:enter,edit:printfindbar,edit:printselection,'
		         'edit:keys:Shift+Enter,edit:keys:Shift+Enter,edit:printfindbar,edit:findbar:text:draw,edit:printfindbar,edit:findbar:case,edit:printfindbar,'
		         'edit:findbar:case,edit:findbar:words,edit:findbar:text:Squ,edit:printfindbar,edit:findbar:words,edit:printfindbar,'
		         'edit:findbar:regex,edit:findbar:text:C\\w+::A,edit:printfindbar,edit:printselection,edit:findbar:text:a(,edit:printfindbar,printsettings,'
		         'edit:key:escape,edit:printfindbar,edit:print'},
		[r'^Find bar: closed, 0 marks$', r'^Find bar: \[Area\] 1 of 2, 2 marks$', r'^Selection: \[Area\] on line 10, caret line 10$',
		 r'^Find bar: \[Area\] 2 of 2, 2 marks$', r'^Selection: \[Area\] on line 21, caret line 21$', r'^Find bar: \[Area\] 2 of 2, 2 marks$',
		 r'^Find bar: \[draw\] 3 of 4, 4 marks$', r'^Find bar: \[draw\] 0 results case, 0 marks$', r'^Find bar: \[Squ\] 0 results words, 0 marks$',
		 r'^Find bar: \[Squ\] 3 of 4, 4 marks$', r'^Find bar: \[C\\w\+::A\] 1 of 2 regex, 2 marks$', r'^Selection: \[CCircle::A\] on line 10, caret line 10$',
		 r'^Find bar: \[a\(\] Not a regular expression: A \( has no \) after it\. regex, 0 marks$', r'<Find MatchCase="false" Words="false" Regex="true"/>',
		 r'^Find bar: closed, 0 marks$', r'Shapes\.cpp \(focus\) line 10: \[int CCircle::Area\(void\)\]'], []),
	('find-bar-replace', {
		#Replace (Control+R): the bar with a second row.  Replace changes the current match and goes on to the next;
		#Replace All changes every match as one edit, which one undo puts back, and the status line says how many.
		#With Regex the replacement's $1 and $2 are the match's groups, and a group the expression hasn't replaces
		#nothing.  Opened with more than a line selected, In Selection is ticked and only what was selected is
		#searched: the field keeps what it had.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '1:1',
		'DEBUG': 'command:Edit.Replace,edit:findbar:text:miSide,edit:findbar:replace:miEdge,edit:printfindbar,edit:findbar:replaceone,edit:printfindbar,edit:printline:23,'
		         'edit:findbar:replaceall,edit:printfindbar,edit:printline:23,printstatus,edit:undo,edit:printline:23,edit:undo,edit:printline:23,'
		         'edit:findbar:regex,edit:findbar:text:C(\\w+)::(\\w+),edit:findbar:replace:$2_$1,edit:findbar:replaceall,edit:printline:4,edit:printline:27,printstatus,'
		         'edit:findbar:text:D(r)aw,edit:findbar:replace:$3,edit:findbar:replaceall,printstatus,'
		         'edit:caret:6:1,edit:selectto:13:1,command:Edit.Replace,edit:printfindbar,edit:findbar:regex,edit:findbar:text:mi,edit:findbar:replace:m_,edit:printfindbar,edit:findbar:replaceall,edit:printline:12,edit:printline:23'},
		[r'^Find bar: \[miSide\] 1 of 2 replace \[miEdge\], 1 marks$', r'^Find bar: \[miSide\] 1 of 1 replace \[miEdge\], 1 marks$', r'^Line 23: \[\treturn miEdge \* miSide;\]$',
		 r'^Find bar: \[miSide\] 0 results replace \[miEdge\], 0 marks$', r'^Line 23: \[\treturn miEdge \* miEdge;\]$', r'^Status line: Replaced 1 occurrence\.$',
		 r'^Line 23: \[\treturn miEdge \* miSide;\]$', r'^Line 23: \[\treturn miSide \* miSide;\]$',
		 r'^Line 4: \[void Draw_Circle\(void\)\]$', r'^Line 27: \[void Draw_RoundedSquare\(void\)\]$', r'^Status line: Replaced 6 occurrences\.$',
		 r"^Status line: Nothing was replaced: The replacement names group \$3, which the pattern doesn't have\.$",
		 r'^Find bar: \[D\(r\)aw\] 0 results regex selection replace \[\$3\], 0 marks$', r'^Find bar: \[mi\] 1 of 2 selection replace \[m_\], 1 marks$',
		 r'^Line 12: \[\treturn 3 \* m_Radius \* m_Radius;\]$', r'^Line 23: \[\treturn miSide \* miSide;\]$'], []),
	('find-next', {
		#Find Next (F3) with nothing searched for says so.  Find Word at Caret (Control+F3) selects the word's next
		#place, and F3 and Shift+F3 go on from there, round the ends, with no bar.  With the bar open they step its
		#matches.  The bar searches again after the file is edited: a match typed into the file is counted.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '4:8',
		'DEBUG': 'command:Edit.FindNext,printstatus,command:Edit.FindWordAtCaret,edit:printselection,command:Edit.FindNext,edit:printselection,command:Edit.FindNext,edit:printselection,'
		         'command:Edit.FindPrevious,edit:printselection,command:Edit.FindPrevious,edit:printselection,'
		         'command:Edit.Find,edit:printfindbar,command:Edit.FindNext,edit:printfindbar,command:Edit.FindPrevious,edit:printfindbar,edit:key:escape,'
		         'edit:focus:editor,edit:caret:6:3,edit:type:x,edit:printline:6,command:Edit.Find,edit:findbar:text:Move,edit:printfindbar,edit:focus:editor,edit:caret:1:1,edit:type:Move ,edit:printfindbar'},
		[r'^Status line: There is nothing to find again: Find \(Control\+F\) or Find Word at Caret \(Control\+F3\) first\.$',
		 r'^Selection: \[CCircle\] on line 10, caret line 10$', r'^Selection: \[CCircle\] on line 4, caret line 4$', r'^Selection: \[CCircle\] on line 10, caret line 10$',
		 r'^Selection: \[CCircle\] on line 4, caret line 4$', r'^Selection: \[CCircle\] on line 10, caret line 10$',
		 r'^Find bar: \[CCircle\] 2 of 2, 2 marks$', r'^Find bar: \[CCircle\] 1 of 2, 2 marks$', r'^Find bar: \[CCircle\] 2 of 2, 2 marks$',
		 r'^Line 6: \[\tMxove\(0, 0\);\]$', r'^Find bar: \[Move\] 0 results, 0 marks$', r'^Find bar: \[Move\] 1 of 1, 1 marks$'], []),
	('find-regex-mask', {
		#Find in Files with Regex and a file mask: the expression's matches, each as long as what it matched, in the
		#source files but for Shape.cpp, which has a CShape::Draw too.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'FIND': '0:r:C\\w+::Draw', 'FIND_MASK': '*.cpp, !Shape.cpp'},
		[r'^Find \[C\\w\+::Draw\]: 4 matches in 1 file', r'^  void CCircle::Draw\(void\)  \[Shapes\.cpp 4\]  match 5\+13$',
		 r'^  void CRoundedSquare::Draw\(void\)  \[Shapes\.cpp 27\]  match 5\+20$', r'^Opened Shapes\.cpp 4:19$',
		 r"^Occurrences of 'C\\w\+::Draw' in the solution  4 matches in 1 file$", r'^    29: CSquare::Draw\(\);$'], [r'Shape\.cpp', r'Shapes\.h', r'ERROR']),
	('find-regex-problem', {
		#What isn't a regular expression finds nothing, and the dialog says what is wrong with it.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'FIND': '0:r:a('},
		[r'^Find \[a\(\]: Not a regular expression: A \( has no \) after it\. in ', r'^Find window: 0 results'], [r'^Opened ', r'ERROR']),
	('replace-in-files', {
		#Replace in Files, listed in the Find tool window: each result shows what it becomes.  Replace Selected on a
		#file's row replaces that file's, as one undo; Replace All the rest; what is left is found again each time,
		#and the files are open and changed, not saved.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'FIND': '0:cw:Area', 'FIND_REPLACE': 'Size', 'FIND_MASK': '*.h',
		'DEBUG': 'printfindtoolbar,findselect:3,findtool:Replace Selected,printstatus,printfind,printtabs,findtool:Replace All,printstatus,printfind,printtabs,edit:printline:13,edit:undo,edit:printline:13'},
		[r'^Find \[Area\]: 3 matches in 2 files', r"^Replace 'Area' with 'Size' in the solution  3 matches in 2 files$", r'^  Shape\.h  1 match$',
		 r'^    15: virtual int  Area\(void\) = 0;  ->  Size$', r'^  Shapes\.h  2 matches$', r'^    24: int  Area\(void\) override;  ->  Size$',
		 r'^Toolbar: Rerun, .*Collapse All, Replace All, Replace Selected, Preview\(on\), Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab$',
		 r'^Status line: Replaced 2 occurrences in 1 file\.  Save All keeps them\.$', r"^Replace 'Area' with 'Size' in the solution  1 match in 1 file$",
		 r'^    15: virtual int  Area\(void\) = 0;  ->  Size$', r'^Tabs: Shape\.h >Shapes\.h\*$',
		 r'^Status line: Replaced 1 occurrence in 1 file\.  Save All keeps them\.$', r"^Replace 'Area' with 'Size' in the solution  0 matches in 0 files$",
		 r'^Tabs: Shape\.h\* >Shapes\.h\*$', r'^Line 13: \[\tint\t\tSize\(void\) override;\]$', r'^Line 13: \[\tint\t\tArea\(void\) override;\]$',
		 r'^Unsaved at exit: 1 file: .*Shape\.h$'], [r'ERROR']),
	('replace-in-files-dialog', {
		#Replace in Files (Control+Shift+R)'s dialog, and its Replace All: it asks, then every match is replaced, a
		#regular expression's group in each, in three files that are opened for it.  Regex is written with the settings.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'FIND_REPLACE': 'Paint_$1', 'DIALOG_ANSWER': 'Replace All|OK', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'findoptions:cr,replace:0:Draw(\\(\\)),answered:2,printstatus,printfind,printtabs,edit:printline:29,printsettings'},
		[r'^Dialog \[Replace in Files\] answered Replace All$', r'^Dialog \[Replace in Files\] answered OK$',
		 r'^Status line: Replaced 4 occurrences in 3 files\.  Save All keeps them\.$', r"^Replace 'Draw\(\\\(\\\)\)' with 'Paint_\$1' in the solution  0 matches in 0 files$",
		 r'^Tabs: Main\.cpp\* Shape\.cpp\* >Shapes\.cpp\*$', r'^Line 29: \[\tCSquare::Paint_\(\);\]$', r'<FindInFiles Scope="Solution" .*MatchCase="true" Words="false" Regex="true" FileMask=""'],
		[r'ERROR']),
	('find-tabs', {
		#The Find tool window's tabs, as Find Usages': a find is shown in the chosen tab, named for its text, unless
		#that tab is pinned (Pin Tab: a pin before its name), when a new tab is made.  A tab keeps its own results, and
		#Rerun looks again for its own text, not what was typed last.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window',
		'DEBUG': 'find:0:Draw,answered:1,edit:findtabs,findtool:Pin Tab,edit:findtabs,printfindtoolbar,find:0:Area,answered:2,edit:findtabs,printfind,'
		         'edit:findtab:* Draw,edit:findtabs,printfind,findtool:Rerun,printfind,edit:findtab:Area,find:0:miSide,answered:3,edit:findtabs'},
		[r'^Find tabs: >Draw$', r'^Find tabs: >\* Draw$', r'^Toolbar: Rerun, .*Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab\(on\)$', r'^Find tabs: \* Draw, >Area$',
		 r"^Occurrences of 'Area' in the solution  9 matches in 4 files$", r'^Find tabs: >\* Draw, Area$', r"^Occurrences of 'Draw' in the solution  15 matches in 5 files$",
		 r"^Occurrences of 'Draw' in the solution  15 matches in 5 files$", r'^Find tabs: \* Draw, >miSide$'], [r'No tool button', r'ERROR']),
	('bookmarks', dict(MakeNavigationCopy(), **{
		#Toggle Bookmark (Control+F11) marks the caret's line: it is listed, shown in the gutter and marked on the
		#scroll bar.  Toggle Bookmark with Mnemonic (Control+Shift+F11) asks for a digit or a letter in a prompt, and
		#says what it wants of anything else.  Next and Previous Bookmark (F2, Shift+F2) go through the file's, round
		#its ends; Control+3 goes to bookmark 3 from another file.  They move with their lines as the file is edited,
		#and are written in the workspace's user's file.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:printbookmarks,command:Bookmarks.Toggle,edit:printbookmarks,edit:caret:23:3,command:Bookmarks.ToggleMnemonic,edit:printprompt,edit:key:type:xx,edit:key:enter,edit:printprompt,'
		         'edit:key:backspace,edit:key:backspace,edit:key:type:3,edit:key:enter,edit:printprompt,edit:printbookmarks,edit:caret:4:1,command:Bookmarks.Toggle,edit:printbookmarks,'
		         'command:Bookmarks.Next,edit:print,command:Bookmarks.Next,edit:print,command:Bookmarks.Next,edit:print,command:Bookmarks.Previous,edit:print,'
		         'edit:open:Shapes.h,command:Bookmarks.Next,printstatus,command:Bookmarks.GoTo5,printstatus,command:Bookmarks.GoTo3,edit:print,'
		         'edit:caret:1:1,edit:tap:Enter,edit:printbookmarks,edit:caret:5:1,command:Bookmarks.Toggle,edit:printbookmarks,printuserfile'}),
		[r'^Bookmarks: 0$', r'^Bookmark gutter:, 0 marks$', r'^Bookmarks: 1$', r'^  Shapes\.cpp:12$', r'^Bookmark gutter: 12\[ \], 1 marks$',
		 r'^Prompt: Mnemonic \(0-9, A-Z\): \[\] $', r'^Prompt: Mnemonic \(0-9, A-Z\): \[xx\] Type one digit or one letter\.$', r'^Prompt: none$',
		 r'^Bookmarks: 2$', r'^  Shapes\.cpp:23 \[3\]$', r'^Bookmark gutter: 12\[ \] 23\[3\], 2 marks$',
		 r'^Bookmarks: 3$', r'^  Shapes\.cpp:4$', r'^  Shapes\.cpp:12$', r'^Bookmark gutter: 4\[ \] 12\[ \] 23\[3\], 3 marks$',
		 r'Shapes\.cpp \(focus\) line 12: ', r'Shapes\.cpp \(focus\) line 23: ', r'Shapes\.cpp \(focus\) line 4: ', r'Shapes\.cpp \(focus\) line 23: ',
		 r'^Status line: This file has no bookmarks: Toggle Bookmark \(Control\+F11\) makes one\.$',
		 r'^Status line: There is no bookmark 5: Toggle Bookmark with Mnemonic \(Control\+Shift\+F11\) makes one\.$', r'Shapes\.cpp \(focus\) line 23: ',
		 r'^Bookmarks: 3$', r'^  Shapes\.cpp:5$', r'^  Shapes\.cpp:13$', r'^  Shapes\.cpp:24 \[3\]$', r'^Bookmark gutter: 5\[ \] 13\[ \] 24\[3\], 3 marks$',
		 r'^Bookmarks: 2$', r'^Bookmark gutter: 13\[ \] 24\[3\], 2 marks$',
		 r'^  <Bookmark File=".*Navigation/Shapes\.cpp" Line="13"/>$', r'^  <Bookmark File=".*Navigation/Shapes\.cpp" Line="24" Mnemonic="3"/>$'], []),
	('bookmarks-read', dict(MakeNavigationCopy('  <Breakpoint File="{folder}/Start.asm" Line="42" Enabled="true"/>\n  <Bookmark File="{folder}/Shapes.cpp" Line="16" Mnemonic="A"/>\n  <Bookmark File="{folder}/Shapes.h" Line="6"/>\n'), **{
		#A workspace's bookmarks are read from its user's file when it opens, beside the breakpoints that are there;
		#a mnemonic given to another line leaves the line that had it, and both parts are written again.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '4:1',
		'DEBUG': 'edit:printbookmarks,command:Bookmarks.ToggleMnemonic,edit:key:type:a,edit:key:enter,edit:printbookmarks,printuserfile'}),
		[r'^Bookmarks: 2$', r'^  Shapes\.cpp:16 \[A\]$', r'^  Shapes\.h:6$', r'^Bookmark gutter: 16\[A\], 1 marks$',
		 r'^Bookmarks: 2$', r'^  Shapes\.cpp:4 \[A\]$', r'^  Shapes\.h:6$', r'^Bookmark gutter: 4\[A\], 1 marks$',
		 r'^  <Breakpoint File=".*Navigation/Start\.asm" Line="42" Enabled="true"/>$', r'^  <Bookmark File=".*Navigation/Shapes\.h" Line="6"/>$',
		 r'^  <Bookmark File=".*Navigation/Shapes\.cpp" Line="4" Mnemonic="A"/>$'], [r'Line="16"']),
	('nav-override-marks', {
		#The gutter's override marks, from the file's outline and the index: ^ beside a function that overrides, v
		#beside one that is overridden and a class with derived classes, x beside one that does both.  A press on ^
		#goes to the super method, on v to the implementation, and on x opens Go to Related Symbol's chooser.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '1:1',
		'DEBUG': 'edit:printmarks,edit:overrideclick:12,edit:print,edit:open:Shapes.h,edit:overrideclick:17,edit:print,edit:open:Shapes.h,edit:overrideclick:23,edit:printchooser,edit:key:escape,edit:open:Shape.h,edit:printmarks'},
		[r'^Marks: 0 usages kept, overrides 12\[\^\] 13\[\^\] 17\[v\] 23\[x\] 24\[\^\] 31\[\^\], 0 scroll marks$',
		 r'Shape\.cpp \(focus\) line 11: \[void CShape::Draw\(void\)\]', r'Shapes\.h \(focus\) line 28: \[class CRoundedSquare : public CSquare\]',
		 r'^Chooser: CSquare::Draw$', r'^   \[Super methods\]$', r'^   CShape::Draw \| Shape\.cpp:11$', r'^   \[Implementations\]$', r'^   CRoundedSquare::Draw \| Shapes\.cpp:27$',
		 r'^Marks: 0 usages kept, overrides 6\[v\] 14\[v\] 15\[v\], 0 scroll marks$'], []),
	('nav-highlight-usages', {
		#Highlight Usages in File (Control+Shift+F7): the caret's symbol's usages in the file are kept highlighted and
		#marked on the scroll bar while the caret goes elsewhere, and the status line says how many.  F3 and Shift+F3
		#step through them; Escape ends it, and F3 has nothing to find again.  On no name, the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '23:10',
		'DEBUG': 'command:Navigate.HighlightUsages,printstatus,edit:printmarks,edit:caret:1:1,edit:printmarks,command:Edit.FindNext,edit:printselection,command:Edit.FindNext,edit:printselection,'
		         'command:Edit.FindNext,edit:printselection,command:Edit.FindPrevious,edit:printselection,edit:key:escape,edit:printmarks,command:Edit.FindNext,printstatus,'
		         'edit:caret:2:1,command:Navigate.HighlightUsages,printstatus'},
		[r'^Status line: 2 usages highlighted: F3 and Shift\+F3 step through them, Escape ends it\.$', r'^Marks: 2 usages kept, .*, 1 scroll marks$',
		 r'^Marks: 2 usages kept, .*, 1 scroll marks$', r'^Selection: \[miSide\] on line 23, caret line 23$', r'^Selection: \[miSide\] on line 23, caret line 23$',
		 r'^Marks: 0 usages kept, .*, 0 scroll marks$', r'^Status line: There is nothing to find again: ',
		 r"^Status line: The caret isn't on a name whose usages in this file are known\.$"], []),
	('nav-next-change', {
		#Next Change and Previous Change (Control+Alt+Shift+Down and Up): the caret goes to the next line changed since
		#the last commit, or the one before; with none that way the status line says so.  The changed lines are marked
		#on the scroll bar.
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'X', 'DIALOG_ANSWER': "Don't Save",
		'DEBUG': 'edit:caret:60:1,edit:type:Y,edit:caret:1:1,command:Navigate.NextChange,edit:print,command:Navigate.NextChange,edit:print,command:Navigate.NextChange,printstatus,'
		         'command:Navigate.PreviousChange,edit:print,command:Navigate.PreviousChange,printstatus,edit:printmarks,exit'},
		[r'CalculatorParser\.cpp \* \(focus\) line 36: ', r'CalculatorParser\.cpp \* \(focus\) line 60: ', r'^Status line: There is no change after the caret\.$',
		 r'CalculatorParser\.cpp \* \(focus\) line 36: ', r'^Status line: There is no change before the caret\.$', r'^Marks: 0 usages kept, overrides, 2 scroll marks$'], []),
	('recent-files', dict(MakeNavigationCopy(), **{
		#Recent Files (Control+E): the files shown in editors but the active one, the one shown before it first, so
		#Enter goes back to it; a closed one is listed after the open ones and opens where its caret was; typing
		#filters.  Reopen Closed Tab opens the tab closed last where its caret was, and is disabled with none.  The
		#workspace's user's file gets them, most recent first, each with its caret's place.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5', 'SAVE_LAYOUT': '1',
		'DEBUG': 'edit:printrecent,command:Navigate.RecentFiles,printstatus,edit:open:Shapes.h,edit:caret:6:2,edit:open:Main.cpp,edit:printrecent,command:Navigate.RecentFiles,edit:printchooser,edit:key:enter,edit:print,'
		         'command:Navigate.RecentFiles,edit:printchooser,edit:key:escape,closetab,edit:printrecent,printtabs,command:Navigate.RecentFiles,edit:printchooser,edit:key:type:sha,edit:printchooser,edit:key:escape,'
		         'command:Window.ReopenClosedTab,edit:print,printtabs,command:Window.ReopenClosedTab,printstatus,edit:printrecent,printuserfile'}),
		[r'^Recent files: Shapes\.cpp$', r'^Status line: There are no other recent files\.$', r'^Recent files: Main\.cpp, Shapes\.h, Shapes\.cpp$',
		 r'^Chooser: Recent Files$', r'^  >Shapes\.h \| .*Navigation$', r'^   Shapes\.cpp \| .*Navigation$', r'Shapes\.h \(focus\) line 6: ',
		 r'^Chooser: Recent Files$', r'^  >Main\.cpp \| ', r'^   Shapes\.cpp \| ', r'^Recent files: Main\.cpp, Shapes\.h:6 \(closed\), Shapes\.cpp$', r'^Tabs: Shapes\.cpp >Main\.cpp$',
		 r'^Chooser: Recent Files$', r'^  >Shapes\.cpp \| ', r'^   Shapes\.h \| ', r'^Chooser: Recent Files$', r'^  >Shapes\.cpp \| ', r'^   Shapes\.h \| ', r'^Chooser filter: \[sha\]$',
		 r'Shapes\.h \(focus\) line 6: ', r'^Tabs: Shapes\.cpp Main\.cpp >Shapes\.h$', r'^Command \[Window\.ReopenClosedTab\] not found, not enabled or did nothing\.$',
		 r'^Recent files: Shapes\.h, Main\.cpp, Shapes\.cpp$',
		 r'^  <RecentFile File=".*Navigation[/\\]Shapes\.h" Line="6" Column="2"/>$', r'^  <RecentFile File=".*Navigation[/\\]Main\.cpp" Line="1" Column="1"/>$',
		 r'^  <RecentFile File=".*Navigation[/\\]Shapes\.cpp" Line="12" Column="5"/>$'], [r'ERROR']),
	('recent-files-read', dict(MakeNavigationCopy('  <RecentFile File="{folder}/Shapes.h" Line="6" Column="2"/>\n  <RecentFile File="{folder}/Shapes.cpp" Line="12" Column="5"/>\n  <RecentFile File="{folder}/Main.cpp" Line="2" Column="1"/>\n'), **{
		#A workspace's recent files are read from its user's file when it opens: with no file open Recent Files lists
		#them all, and one chosen opens where its caret was.
		'WAIT_INDEX': '1', 'SAVE_LAYOUT': '1',
		'DEBUG': 'edit:printrecent,command:Navigate.RecentFiles,edit:printchooser,edit:key:down,edit:key:enter,edit:print,edit:printrecent,printuserfile'}),
		[r'^Recent files: Shapes\.h:6 \(closed\), Shapes\.cpp:12 \(closed\), Main\.cpp:2 \(closed\)$',
		 r'^Chooser: Recent Files$', r'^  >Shapes\.h \| ', r'^   Shapes\.cpp \| ', r'^   Main\.cpp \| ', r'Shapes\.cpp \(focus\) line 12: ',
		 r'^Recent files: Shapes\.cpp, Shapes\.h:6 \(closed\), Main\.cpp:2 \(closed\)$',
		 r'^  <RecentFile File=".*Navigation/Shapes\.cpp" Line="12" Column="5"/>$', r'^  <RecentFile File=".*Navigation/Shapes\.h" Line="6" Column="2"/>$',
		 r'^  <RecentFile File=".*Navigation/Main\.cpp" Line="2" Column="1"/>$'], [r'ERROR']),
	('recent-files-not-kept', {
		#A run that doesn't keep settings leaves no user's file beside the solution for the files it showed.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shapes.h,edit:printrecent,printuserfile'},
		[r'^Recent files: Shapes\.h, Shapes\.cpp$', r'^User file: none$'], [r'RecentFile']),
	('recent-locations', dict(MakeNavigationCopy(), **{
		#Recent Locations (Control+Shift+E): the places Back has, the newest first, each its line's text with its file
		#and line, previewed; typing filters.  Its button lists only the places edited (all of them, the status line
		#saying so, while nothing has been), and back; one chosen is gone to, which Back remembers.
		'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '1:1',
		'DEBUG': 'command:Navigate.RecentLocations,printstatus,edit:caret:6:19,gotosource,edit:open:Main.cpp,edit:caret:24:3,gotosource,edit:printhistory,command:Navigate.RecentLocations,edit:printchooser,'
		         'edit:chooserbutton,printstatus,edit:printchooser,edit:key:type:area,edit:printchooser,edit:key:escape,'
		         'edit:caret:20:1,edit:tap:Enter,edit:caret:2:1,edit:key:type:x,edit:printrecent,command:Navigate.RecentLocations,edit:chooserbutton,edit:printchooser,edit:key:down,edit:key:enter,edit:print,edit:printhistory,'
		         'command:Navigate.RecentLocations,edit:printchooser,edit:chooserbutton,edit:printchooser,edit:key:escape'}),
		[r'^Status line: There are no recent locations yet\.$', r'^History: back \[Main\.cpp:6, Shape\.h:15, Main\.cpp:24\] forward \[\]$',
		 r'^Chooser: Recent Locations$', r'^  >DrawAll\(&cCircle, &cCircle\); \| Main\.cpp:24$', r'^   virtual int Area\(void\) = 0; \| Shape\.h:15$',
		 r'^   return pcFirst->Area\(\) \+ pcSecond->Area\(\); \| Main\.cpp:6$', r'^Chooser preview: Main\.cpp$',
		 r'^Status line: Nothing has been edited yet: every recent location is listed\.$', r'^Chooser: Recent Locations$', r'^  >DrawAll',
		 r'^Chooser: Recent Locations$', r'^  >virtual int Area\(void\) = 0; \| Shape\.h:15$', r'^   return pcFirst', r'^Chooser filter: \[area\]$', r'^Chooser preview: Shape\.h$',
		 r'^Edit locations: 2 at 2$', r'^Chooser: Recent Locations: Changed$', r'^  >x \| Main\.cpp:2$', r'^   CRoundedSquare cSquare; \| Main\.cpp:21$',
		 r'Main\.cpp \* \(focus\) line 21: ', r'^History: back \[Main\.cpp:6, Shape\.h:15, Main\.cpp:25, Main\.cpp:2\] forward \[\]$',
		 r'^Chooser: Recent Locations: Changed$', r'^  >x \| Main\.cpp:2$', r'^Chooser: Recent Locations$', r'^  >x \| Main\.cpp:2$', r'^   DrawAll\(&cCircle, &cCircle\); \| Main\.cpp:25$',
		 r'^   virtual int Area\(void\) = 0; \| Shape\.h:15$'], [r'ERROR', r'not found, not enabled']),
	('edit-locations', dict(MakeNavigationCopy(), **{
		#Last Edit Location (Control+Shift+Backspace) goes back through the places edited, an edit near the last one
		#being the same place, skipping the newest while the caret is still on it; Next Edit Location comes forward.
		#Each says so at its end.  Back returns from one.
		'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '1:1',
		'DEBUG': 'command:Navigate.LastEditLocation,printstatus,edit:caret:6:1,edit:tap:Enter,edit:caret:8:1,edit:tap:Enter,edit:printrecent,edit:caret:20:1,edit:tap:Enter,edit:open:Shapes.cpp,edit:caret:12:1,edit:tap:Enter,edit:printrecent,'
		         'command:Navigate.LastEditLocation,edit:print,command:Navigate.LastEditLocation,edit:print,command:Navigate.LastEditLocation,printstatus,'
		         'command:Navigate.NextEditLocation,edit:print,command:Navigate.NextEditLocation,edit:print,command:Navigate.NextEditLocation,printstatus,edit:printrecent,command:Navigate.Back,edit:print'}),
		[r'^Status line: Nothing has been edited yet\.$', r'^Edit locations: 1 at 1$', r'^Edit locations: 3 at 3$',
		 r'Main\.cpp \* \(focus\) line 21: ', r'Main\.cpp \* \(focus\) line 9: ', r'^Status line: There is no edit location before this one\.$',
		 r'Main\.cpp \* \(focus\) line 21: ', r'Shapes\.cpp \* \(focus\) line 13: ', r'^Status line: There is no edit location after this one\.$',
		 r'^Edit locations: 3 at 2$', r'Main\.cpp \* \(focus\) line 21: '], [r'ERROR']),
	('switcher', {
		#The Switcher (Control+Tab): held open, the open files most recent first then the tool windows, the file before
		#this one selected; each Control+Tab moves on and Control+Shift+Tab back, round the ends, and letting Control go
		#chooses.  A quick Control+Tab goes straight to the file before this one.  Select Next Tab and Select Previous
		#Tab (Alt+Right, Alt+Left) go along the tabs, round the ends.  A tool window chosen is shown.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shapes.h,edit:open:Main.cpp,edit:switcher:hold,edit:printchooser,command:Window.Switcher,edit:printchooser,command:Window.SwitcherBackward,command:Window.SwitcherBackward,edit:printchooser,'
		         'command:Window.Switcher,edit:switcher:release,printtabs,command:Window.Switcher,printtabs,command:Window.Switcher,printtabs,'
		         'command:Window.NextTab,printtabs,command:Window.NextTab,printtabs,command:Window.PreviousTab,printtabs,'
		         'edit:switcher:hold,command:Window.SwitcherBackward,command:Window.SwitcherBackward,edit:printchooser,edit:switcher:release,edit:print,edit:printrecent'},
		[r'^Chooser: Switcher$', r'^   Main\.cpp \| ', r'^  >Shapes\.h \| ', r'^   Shapes\.cpp \| ', r'^   \[Tool Windows\]$', r'^   Solution Explorer \| $', r'^   Structure \| $', r'^   Claude \| $',
		 r'^Chooser: Switcher$', r'^  >Shapes\.cpp \| ', r'^   \[Tool Windows\]$', r'^Chooser: Switcher$', r'^  >Main\.cpp \| ',
		 r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$', r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$', r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$',
		 r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$', r'^Tabs: >Shapes\.cpp Shapes\.h Main\.cpp$', r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$',
		 r'^Chooser: Switcher$', r'^   Main\.cpp \| ', r'^   \[Tool Windows\]$', r'^  >Claude \| $', r'Main\.cpp line 1: ', r'^Recent files: Main\.cpp, Shapes\.cpp, Shapes\.h$'],
		[r'ERROR', r'not found, not enabled', r'\(focus\) line']),
	('switcher-keys', {
		#The Switcher with real keys: Control+Tab opens it and it stays while Control is down, Tab and Control+Up moving
		#the selection; letting Control go chooses.  Control+Tab let go at once goes to the file before.  Control+Shift+
		#Tab opens it at its last row, a tool window, which is shown: no editor has the keys.  Alt+Left is the tab to the
		#left, and Control+E lists the recent files.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shapes.h,edit:open:Main.cpp,edit:press:Left Control,edit:tap:Tab,edit:printchooser,edit:tap:Tab,edit:tap:Cursor Up,edit:release:Left Control,edit:printchooser,printtabs,edit:print,'
		         'edit:press:Left Control,edit:tap:Tab,edit:release:Left Control,printtabs,edit:press:Left Control,edit:press:Left Shift,edit:tap:Tab,edit:release:Left Shift,edit:tap:Cursor Up,edit:tap:Cursor Up,edit:printchooser,'
		         'edit:release:Left Control,printtabs,edit:press:Left Alt,edit:tap:Cursor Left,edit:release:Left Alt,printtabs,edit:press:Left Control,edit:tap:E,edit:release:Left Control,edit:printchooser,edit:tap:Enter,edit:print'},
		[r'^Chooser: Switcher$', r'^  >Shapes\.h \| ', r'^Chooser: none$', r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$', r'Shapes\.h \(focus\) line 1: ',
		 r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$', r'^Chooser: Switcher$', r'^  >Watch 3 \| $', r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$', r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$',
		 r'^Chooser: Recent Files$', r'^  >Main\.cpp \| ', r'^   Shapes\.cpp \| ', r'Main\.cpp \(focus\) line 1: '], [r'ERROR']),
	('switcher-close', {
		#Delete in the Switcher closes the selected file and leaves the Switcher open, the row that took its place
		#selected; on a tool window's row it does nothing.  With real keys: Control+Tab, Delete, and letting Control go
		#chooses the row that is selected then.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shapes.h,edit:open:Shape.h,edit:open:Main.cpp,edit:switcher:hold,edit:printchooser,edit:switcher:close,edit:printchooser,printtabs,'
		         'command:Window.Switcher,command:Window.Switcher,command:Window.Switcher,edit:switcher:close,edit:printchooser,edit:switcher:release,printtabs,'
		         'edit:press:Left Control,edit:tap:Tab,edit:printchooser,edit:tap:Delete,edit:printchooser,edit:release:Left Control,printtabs,edit:print'},
		[r'^Chooser: Switcher$', r'^   Main\.cpp \| ', r'^  >Shape\.h \| ', r'^   Shapes\.h \| ', r'^   Shapes\.cpp \| ',
		 r'^Chooser: Switcher$', r'^   Main\.cpp \| ', r'^  >Shapes\.h \| ', r'^   Shapes\.cpp \| ', r'^   \[Tool Windows\]$', r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$',
		 r'^Switcher: nothing to close$', r'^Chooser: Switcher$', r'^  >Structure \| $', r'^Tabs: Shapes\.cpp Shapes\.h >Main\.cpp$',
		 r'^Chooser: Switcher$', r'^  >Shapes\.h \| ', r'^Chooser: Switcher$', r'^   Main\.cpp \| ', r'^  >Shapes\.cpp \| ', r'^   \[Tool Windows\]$',
		 r'^Tabs: >Shapes\.cpp Main\.cpp$', r'Shapes\.cpp \(focus\) line 12: '], [r'ERROR']),
	('recent-files-edited', {
		#Recent Files' button, Show Edited Only, lists only the files edited this session, and Show All every one
		#again.  When none of the others was edited, all are listed and the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shape.h,edit:open:Shapes.cpp,edit:caret:12:5,edit:type:x,edit:open:Shapes.h,edit:open:Main.cpp,command:Navigate.RecentFiles,edit:printchooser,'
		         'edit:chooserbutton,edit:printchooser,edit:chooserbutton,edit:printchooser,edit:key:escape,'
		         'edit:open:Shapes.cpp,command:Navigate.RecentFiles,edit:chooserbutton,edit:printchooser,printstatus'},
		[r'^Chooser: Recent Files$', r'^  >Shapes\.h \| ', r'^   Shapes\.cpp \| ', r'^   Shape\.h \| ',
		 r'^Chooser: Recent Files: Edited$', r'^  >Shapes\.cpp \| ', r'^Chooser filter: \[\]$',
		 r'^Chooser: Recent Files$', r'^  >Shapes\.h \| ', r'^   Shapes\.cpp \| ', r'^   Shape\.h \| ',
		 r'^Chooser: Recent Files$', r'^  >Main\.cpp \| ', r'^   Shapes\.h \| ', r'^Status line: No other recent file has been edited: every recent file is listed\.$'],
		[r'ERROR']),
	('select-in', {
		#Select In (Alt+F1): a chooser of where the active file can be shown selected.  Solution Explorer selects it
		#in the tree, which gets the keys; Structure shows the file's outline with the caret's entry selected.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'command:Navigate.SelectIn,edit:printchooser,edit:key:enter,printexplorer,edit:print,edit:focus:editor,command:Navigate.SelectIn,edit:key:down,edit:key:enter,edit:printstructure'},
		[r'^Chooser: Select In$', r'^  >Solution Explorer \| $', r'^   Structure \| $', r'^Explorer: \[.*Navigation/Shapes\.cpp\]$', r'Shapes\.cpp line 12: ',
		 r'^Structure: 5 rows$', r'^> CCircle::Area\(void\)$'], [r'ERROR', r'not found, not enabled']),
	('explorer-options', {
		#Solution Explorer's menu's Always Select Opened File selects the file of the editor shown in the tree as tabs
		#are chosen (the checks' own opening of a file selects it there anyway), and Open Files with Single Click has
		#one click open a file; both are written with the settings, and neither once both are off.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'edit:open:Shapes.h,edit:open:Main.cpp,command:Window.PreviousTab,printtabs,printexplorer,command:Explorer.AlwaysSelectOpenedFile,printexplorer,command:Window.PreviousTab,printexplorer,'
		         'command:Explorer.OpenFilesWithSingleClick,printexplorer,edit:print,printsettings,command:Explorer.AlwaysSelectOpenedFile,command:Explorer.OpenFilesWithSingleClick,command:Window.NextTab,printtabs,printexplorer,printsettings'},
		[r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$', r'^Explorer: \[.*Navigation/Main\.cpp\]$', r'^Explorer: \[.*Navigation/Shapes\.h\]$', r'^Explorer options: always select$',
		 r'^Explorer: \[.*Navigation/Shapes\.cpp\]$', r'^Explorer options: always select$', r'^Explorer: \[.*Navigation/Shapes\.cpp\]$', r'^Explorer options: always select single click$',
		 r'Shapes\.cpp \(focus\) line 12: ', r'<Explorer AlwaysSelect="true" SingleClick="true"/>', r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$',
		 r'^Explorer: \[.*Navigation/Shapes\.cpp\]$', r'^Settings file:$', r'^</Settings>$'], [r'ERROR', r'not found, not enabled']),
	('explorer-options-read', {
		#The settings as they were written: the tree follows the editor shown and opens with one click.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <Explorer AlwaysSelect="true" SingleClick="true"/>\n</Settings>\n'),
		'DEBUG': 'edit:open:Shapes.h,edit:open:Main.cpp,command:Window.PreviousTab,printexplorer'},
		[r'^Explorer: \[.*Navigation/Shapes\.h\]$', r'^Explorer options: always select single click$'], [r'ERROR']),
	('quick-link', {
		#With Control held over a name the name is underlined and its declaration shown under it, as it is written,
		#with where it is declared: a member function's as its class declares it (the one of two of a name that the
		#call is of), a function's, an enumerator's, a parameter's, a local variable's, a class's, a field's with its
		#array's bounds, a typedef's and a static variable's.  A macro has its #define's line; a keyword and a number
		#have none.
		'SOLUTION': QUICK_INFO, 'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp', 'CARET': '1:1',
		'DEBUG': 'edit:printinfo,edit:linkhover:51:15,edit:printinfo,edit:linkhover:54:15,edit:printinfo,edit:linkhover:51:23,edit:printinfo,edit:linkhover:51:53,edit:printinfo,edit:linkhover:51:4,edit:printinfo,'
		         'edit:linkhover:45:8,edit:printinfo,edit:linkhover:43:14,edit:printinfo,edit:linkhover:21:3,edit:printinfo,edit:linkhover:7:21,edit:printinfo,edit:linkhover:4:20,edit:printinfo,'
		         'edit:linkhover:31:22,edit:printinfo,edit:linkhover:27:9,edit:printinfo,edit:linkhover:49:3,edit:printinfo,edit:linkhover:51:46,edit:printinfo,edit:linkhover:9:3,edit:printinfo,edit:linkhover:off,edit:printinfo'},
		[r'^Link: none$', r'^Link: \[Rect\] void Rect\(int iX, int iY, int iWidth, int iHeight, EPaintMode eMode = PM_Fill\) \| CPainter, Paint\.h:28$',
		 r'^Link: \[Rect\] void Rect\(int iX, int iY\) \| CPainter, Paint\.h:27$', r'^Link: \[Blend\] int Blend\(int iFirst, int iSecond, float fAmount\) \| Paint\.h:34$',
		 r'^Link: \[PM_Outline\] PM_Outline = 4 \| EPaintMode, Paint\.h:11$', r'^Link: \[pcPainter\] CPainter\* pcPainter \| Paint\.cpp:43$', r'^Link: \[iTotal\] int iTotal \| Paint\.cpp:45$',
		 r'^Link: \[CPainter\] class CPainter \| Paint\.h:19$', r'^Link: \[maiSizes\] int maiSizes\[PAINT_MAXIMUM\] \| CPainter, Paint\.h:23$',
		 r'^Link: \[PaintColour\] typedef unsigned int PaintColour \| Paint\.h:15$', r'^Link: \[gcDefaultPainter\] static CPainter gcDefaultPainter \| Paint\.cpp:4$',
		 r'^Link: \[Default\] static CPainter\* Default\(void\) \| CPainter, Paint\.h:30$', r'^Link: \[PAINT_MAXIMUM\] #define PAINT_MAXIMUM 8 \| Paint\.h:5$', r'^Link: none$', r'^Link: none$',
		 r'^Link: \[muiColour\] PaintColour muiColour \| CPainter, Paint\.h:22$', r'^Link: none$'], [r'ERROR', r'^Info: (parameters|type)']),
	('quick-link-pointer', {
		#The same with the pointer and the keys themselves: nothing until Control is down, then the name under the
		#pointer as it moves (the one just before it, on a comma), nothing once Shift is down too or Control is let
		#go; and Control+click still goes to the name's source.  The pointer is a hand while a name is a link, and
		#the text's I-beam otherwise.
		'SOLUTION': QUICK_INFO, 'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp', 'CARET': '51:1',
		'DEBUG': 'edit:printcrumbs,edit:pointer:51:15,edit:printinfo,edit:press:Left Control,edit:printinfo,edit:pointer:51:23,edit:printinfo,edit:pointer:51:20,edit:printinfo,edit:pointer:52:25,edit:printinfo,'
		         'edit:press:Left Shift,edit:printinfo,edit:release:Left Shift,edit:printinfo,edit:release:Left Control,edit:printinfo,'
		         'edit:press:Left Control,edit:controlclick:51:23,edit:release:Left Control,edit:printinfo,edit:print'},
		[r'^Link: none$', r'^Pointer: text, text over it$',
		 r'^Link: \[Rect\] void Rect\(int iX, int iY, int iWidth, int iHeight, EPaintMode eMode = PM_Fill\) \| CPainter, Paint\.h:28$', r'^Pointer: hand, hand over it$',
		 r'^Link: \[Blend\] int Blend\(int iFirst, int iSecond, float fAmount\) \| Paint\.h:34$', r'^Link: \[i\] int i \| Paint\.cpp:46$',
		 r'^Link: \[Count\] int Count\(void\) const \| CPainter, Paint\.h:29$', r'^Link: none$', r'^Pointer: text, text over it$',
		 r'^Link: \[Count\] int Count\(void\) const \| CPainter, Paint\.h:29$', r'^Pointer: hand, hand over it$', r'^Link: none$', r'^Pointer: text, text over it$',
		 r'^Link: none$', r'Paint\.cpp \(focus\) line 37: \[int Blend\(int iFirst, int iSecond, float fAmount\)\]'], [r'ERROR', r'^Info: (parameters|type)']),
	('quick-parameters', {
		#Parameter Info (Control+P): the parameters of the call the caret is in, the function called first and the
		#other of its name after it, the caret's argument's parameter marked; it follows the caret into a call
		#inside the call, on along the arguments and to another call, stays as an argument is typed, and goes when
		#the caret leaves every call or on Escape.  Outside a call, and in a for's brackets, the status line says so.
		'SOLUTION': QUICK_INFO, 'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp', 'CARET': '51:19',
		'DEBUG': 'command:View.ParameterInfo,edit:printinfo,edit:caret:51:31,edit:printinfo,edit:caret:51:46,edit:printinfo,edit:caret:51:52,edit:printinfo,edit:caret:54:27,edit:printinfo,'
		         'edit:key:type:1,edit:printinfo,edit:key:backspace,edit:caret:52:30,edit:printinfo,edit:caret:48:5,edit:printinfo,command:View.ParameterInfo,printstatus,edit:caret:49:8,command:View.ParameterInfo,printstatus,'
		         'edit:caret:15:7,command:View.ParameterInfo,edit:printinfo,edit:key:escape,edit:printinfo,edit:print'},
		[r'^Info: parameters$', r'^  \[int iX\], int iY, int iWidth, int iHeight, EPaintMode eMode = PM_Fill$', r'^  \[int iX\], int iY$',
		 r'^Info: parameters$', r'^  int iFirst, \[int iSecond\], float fAmount$', r'^Info: parameters$', r'^  int iX, int iY, \[int iWidth\], int iHeight, EPaintMode eMode = PM_Fill$', r'^  int iX, int iY$',
		 r'^Info: parameters$', r'^  int iX, int iY, int iWidth, int iHeight, \[EPaintMode eMode = PM_Fill\]$', r'^Info: parameters$', r'^  int iX, \[int iY\]$',
		 r'^  int iX, \[int iY\], int iWidth, int iHeight, EPaintMode eMode = PM_Fill$', r'^Info: parameters$', r'^  int iX, \[int iY\]$', r'^Info: parameters$', r'^  <no parameters>$',
		 r'^Info: none$', r"^Status line: The caret isn.t among a call.s arguments\.$", r"^Status line: The caret isn.t among a call.s arguments\.$",
		 r'^Info: parameters$', r'^  \[int iX\], int iY, int iWidth, int iHeight, EPaintMode eMode = PM_Fill$', r'^Info: none$', r'Paint\.cpp \* \(focus\) line 15: '], [r'ERROR', r'^Link: \[']),
	('quick-type', {
		#Type Info (Control+Shift+P): the type the symbol at the caret is declared with (a local variable's, a
		#parameter's, what a function returns, a field's with its array's bounds, a class as it is declared), or the
		#selected expression's; it goes when the caret moves.  With neither, the status line says so.
		'SOLUTION': QUICK_INFO, 'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp', 'CARET': '52:5',
		'DEBUG': 'command:View.TypeInfo,edit:printinfo,edit:caret:51:5,edit:printinfo,command:View.TypeInfo,edit:printinfo,edit:caret:52:25,command:View.TypeInfo,edit:printinfo,edit:caret:21:3,command:View.TypeInfo,edit:printinfo,'
		         'edit:caret:31:22,command:View.TypeInfo,edit:printinfo,edit:caret:52:13,edit:selectto:52:31,command:View.TypeInfo,edit:printinfo,edit:caret:43:14,command:View.TypeInfo,edit:printinfo,'
		         'edit:caret:47:1,command:View.TypeInfo,printstatus,edit:printinfo'},
		[r'^Info: type$', r'^  int$', r'^Info: none$', r'^Info: type$', r'^  CPainter\*$', r'^Info: type$', r'^  int$', r'^Info: type$', r'^  int\[PAINT_MAXIMUM\]$', r'^Info: type$', r'^  CPainter\*$',
		 r'^Info: type$', r'^  int$', r'^Info: type$', r'^  class CPainter$', r'^Status line: No type is known for what is at the caret\.$', r'^Info: none$'], [r'ERROR', r'^Link: \[']),
	('quick-definition', {
		#Quick Definition (Control+Shift+I): the definition of the symbol at the caret in a chooser that previews its
		#code; Enter goes to it.  With no symbol there the status line says so.
		'SOLUTION': QUICK_INFO, 'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp', 'CARET': '51:15',
		'DEBUG': 'command:View.QuickDefinition,edit:printchooser,edit:key:escape,edit:print,command:View.QuickDefinition,edit:key:enter,edit:print,edit:caret:47:1,command:View.QuickDefinition,printstatus'},
		[r'^Chooser: Definition of CPainter::Rect$', r'^  >CPainter::Rect \| Paint\.cpp:19$', r'^Chooser preview: Paint\.cpp$', r'Paint\.cpp \(focus\) line 51: ',
		 r'Paint\.cpp \(focus\) line 19: \[void CPainter::Rect\(int iX, int iY, int iWidth, int iHeight, EPaintMode eMode\)\]',
		 r'^Status line: There is no symbol at the caret to show the definition of\.$'], [r'ERROR']),
	('quick-definition-several', {
		#Quick Definition of a virtual function lists its definition and what implements it.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '12:12',
		'DEBUG': 'command:View.QuickDefinition,edit:printchooser,edit:key:escape'},
		[r'^Chooser: Definition of CShape::Draw  4$', r'^  >CShape::Draw \| Shape\.cpp:\d+$', r'CCircle::Draw \| Shapes\.cpp:4$', r'CSquare::Draw \| Shapes\.cpp:16$', r'CRoundedSquare::Draw \| Shapes\.cpp:27$'], [r'ERROR']),
	('hierarchy-type', {
		#Type Hierarchy (Control+H): the class at the caret under its base class with the classes derived from it under
		#it, in the Hierarchy tool window; the toolbar shows its base classes alone or its derived classes alone, the
		#buttons for calls dim.  Enter goes to the selected class.  From a member, its class's; a pinned tab is left
		#and a new one made.  Where there is no class the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.h', 'CARET': '17:8',
		'DEBUG': 'edit:printhierarchy,command:Navigate.TypeHierarchy,edit:printhierarchy,edit:printhierarchytoolbar,edit:hierarchytool:Supertypes Hierarchy,edit:printhierarchy,edit:hierarchytool:Subtypes Hierarchy,'
		         'edit:printhierarchy,edit:key:down,edit:key:enter,edit:print,edit:hierarchytabs,edit:hierarchytool:Pin Tab,edit:open:Main.cpp,edit:caret:6:19,command:Navigate.TypeHierarchy,edit:printhierarchy,edit:hierarchytabs,'
		         'edit:focus:editor,edit:caret:1:1,command:Navigate.TypeHierarchy,printstatus'},
		[r'^Hierarchy: none$', r'^Hierarchy: Type: CSquare \(class hierarchy\)$', r'^  CShape  Shape\.h$', r'^ >  CSquare  Shapes\.h$', r'^      CRoundedSquare  Shapes\.h$',
		 r'^Toolbar: Rerun, Previous Occurrence, Next Occurrence, Expand All, Collapse All, Class Hierarchy\(on\), Supertypes Hierarchy, Subtypes Hierarchy, Caller Hierarchy\(disabled\), Callee Hierarchy\(disabled\), Preview\(on\), Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab$',
		 r'^Hierarchy: Type: CSquare \(supertypes\)$', r'^ >CSquare  Shapes\.h$', r'^    CShape  Shape\.h$', r'^Hierarchy: Type: CSquare \(subtypes\)$', r'^ >CSquare  Shapes\.h$', r'^    CRoundedSquare  Shapes\.h$',
		 r'Shapes\.h \(focus\) line 28: \[class CRoundedSquare : public CSquare\]', r'^Hierarchy tabs: >Type: CSquare$',
		 r'^Hierarchy: Type: CShape \(class hierarchy\)$', r'^ >CShape  Shape\.h$', r'^    CCircle  Shapes\.h$', r'^    CSquare  Shapes\.h$', r'^      CRoundedSquare  Shapes\.h$',
		 r'^Hierarchy tabs: \* Type: CSquare, >Type: CShape$', r'^Status line: There is no class at the caret to show the hierarchy of\.$'], [r'ERROR', r'not found, not enabled']),
	('hierarchy-method', {
		#Method Hierarchy (Control+Shift+H): the classes of the member function at the caret, each saying whether it
		#only declares it (abstract), defines it (+) or inherits it (-); Enter on a class that defines it goes to its
		#function, on one that doesn't to the class.  From anywhere inside a function, that function's.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '21:15',
		'DEBUG': 'command:Navigate.MethodHierarchy,edit:printhierarchy,edit:key:enter,edit:print,command:View.Hierarchy,edit:key:down,edit:key:enter,edit:print,'
		         'edit:open:Shapes.cpp,edit:caret:28:1,command:Navigate.MethodHierarchy,edit:printhierarchy,edit:key:up,edit:key:up,edit:key:enter,edit:print,'
		         'edit:open:Main.cpp,edit:caret:11:1,command:Navigate.MethodHierarchy,printstatus'},
		[r'^Hierarchy: Method: CSquare::Area \(class hierarchy\)$', r'^  CShape  abstract  Shape\.h$', r'^ >  CSquare  \+  Shapes\.h$', r'^      CRoundedSquare  -  Shapes\.h$',
		 r'Shapes\.cpp \(focus\) line 21: \[int CSquare::Area\(void\)\]', r'Shapes\.h \(focus\) line 28: \[class CRoundedSquare : public CSquare\]',
		 r'^Hierarchy: Method: CRoundedSquare::Draw \(class hierarchy\)$', r'^  CShape  \+  Shape\.h$', r'^    CSquare  \+  Shapes\.h$', r'^ >    CRoundedSquare  \+  Shapes\.h$',
		 r'Shape\.cpp \(focus\) line 11: \[void CShape::Draw\(void\)\]', r'^Status line: There is no member function at the caret to show the hierarchy of\.$'], [r'ERROR']),
	('hierarchy-calls', {
		#Call Hierarchy (Control+Alt+H): the function at the caret, or the one the caret is in, with the functions that
		#call it under it, each closed until it is opened, when its own callers are found; Enter goes to the call.
		#The toolbar turns it to what the function calls, in the order it calls them, with how many calls of each.
		#Expand All shows a row standing for what hasn't been looked for, and Enter on it looks.  Next Occurrence steps
		#through the rows; Rerun makes it again.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '4:15',
		'DEBUG': 'command:Navigate.CallHierarchy,edit:printhierarchy,edit:hierarchyexpand:DrawAll,edit:printhierarchy,edit:hierarchyexpand:main,edit:printhierarchy,edit:key:enter,edit:print,'
		         'edit:caret:19:1,command:Navigate.CallHierarchy,edit:printhierarchy,edit:printhierarchytoolbar,edit:hierarchytool:Callee Hierarchy,edit:printhierarchy,edit:hierarchyexpand:DrawAll,edit:printhierarchy,'
		         'edit:focus:editor,command:Navigate.NextOccurrence,edit:print,edit:open:Shape.cpp,edit:caret:18:1,command:Navigate.CallHierarchy,edit:hierarchytool:Expand All,edit:printhierarchy,'
		         'edit:key:down,edit:key:down,edit:key:enter,edit:printhierarchy,edit:hierarchytool:Rerun,edit:printhierarchy,edit:open:Shapes.h,edit:caret:9:7,command:Navigate.CallHierarchy,printstatus'},
		[r'^Hierarchy: Calls: CCircle::Draw \(callers\)$', r'^ >CCircle::Draw$', r'^    \+ DrawAll  Main\.cpp:13$', r'^ >  DrawAll  Main\.cpp:13$', r'^      \+ main  Main\.cpp:24$',
		 r'^  CCircle::Draw$', r'^    DrawAll  Main\.cpp:13$', r'^ >    main  Main\.cpp:24$', r'Main\.cpp \(focus\) line 24: \[\tDrawAll\(&cCircle, &cCircle\);\]',
		 r'^Hierarchy: Calls: main \(callers\)$', r'^ >main$',
		 r'^Toolbar: Rerun, Previous Occurrence\(disabled\), Next Occurrence\(disabled\), Expand All, Collapse All, Class Hierarchy\(disabled\), Supertypes Hierarchy\(disabled\), Subtypes Hierarchy\(disabled\), Caller Hierarchy\(on\), Callee Hierarchy, Preview\(on\), Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab$',
		 r'^Hierarchy: Calls: main \(callees\)$', r'^ >main$', r'^    \+ CShape::Init  2 calls  Main\.cpp:22$', r'^    \+ DrawAll  Main\.cpp:24$', r'^    \+ TotalArea  Main\.cpp:25$',
		 r'^ >  DrawAll  Main\.cpp:24$', r'^      \+ CShape::Draw  Main\.cpp:12$', r'^      \+ CCircle::Draw  Main\.cpp:13$', r'^    \+ TotalArea  Main\.cpp:25$', r'Main\.cpp \(focus\) line 12: \[\tpcShape->Draw\(\);\]',
		 r'^Hierarchy: Calls: CShape::Move \(callers\)$', r'^ >CShape::Move$', r'^    CCircle::Draw  Shapes\.cpp:6$', r'^      \.\.\.$',
		 r'^Hierarchy: Calls: CShape::Move \(callers\)$', r'^ >  CCircle::Draw  Shapes\.cpp:6$', r'^      \+ DrawAll  Main\.cpp:13$',
		 r'^Hierarchy: Calls: CShape::Move \(callers\)$', r'^ >CShape::Move$', r'^    \+ CCircle::Draw  Shapes\.cpp:6$', r'^Status line: There is no function at the caret to show the calls of\.$'], [r'ERROR', r'No hierarchy row']),
	('goto-recent-preview', {
		#The Go to dialog previews its selected result's code, and with nothing typed lists what was gone to last
		#first: on the Files tab the recent files, the one shown before this one at the top; on the Classes and
		#Symbols tabs what each went to before.  A line typed after a file's name is the line previewed and gone to.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shapes.h,edit:open:Main.cpp,edit:gotolist:1:,printtabs,edit:gotolist:0:,edit:gotolist:0:CSquare,edit:gotolist:0:,edit:gotolist:2:Area,edit:gotolist:2:,edit:gotolist:1:Shape.h:15,edit:print'},
		[r'^Go to \[\]: 6 results', r'^  Shapes\.h  \[.*Navigation\]  score 1000000$', r'^  Shapes\.cpp  \[.*Navigation\]  score 999999$', r'^Go to preview: Shapes\.h$',
		 r'^Tabs: Shapes\.cpp >Shapes\.h Main\.cpp$', r'^Go to \[\]: 5 results', r'^  CShape  \[Shape\.h:6\]  score -\d+$', r'^Go to preview: Shape\.h$',
		 r'^Go to \[CSquare\]: 2 results', r'^  CSquare  \[Shapes\.h:17\]', r'^Go to preview: Shapes\.h$', r'^Go to \[\]: 5 results', r'^  CSquare  \[Shapes\.h:17\]  score 1000000$',
		 r'^  CShape  \[Shape\.h:6\]  score 999999$', r'^Go to \[Area\]: 4 results', r'^  Area\(\)  \[CCircle  Shapes\.cpp:10\]', r'^Go to preview: Shapes\.cpp$',
		 r'^Go to \[\]: 26 results', r'^  Area\(\)  \[CCircle  Shapes\.cpp:10\]  score 1000000$', r'^Go to \[Shape\.h:15\]: 1 results', r'^Go to preview: Shape\.h$',
		 r'Shape\.h \(focus\) line 15: \[\tvirtual int\t\tArea\(void\) = 0;\]'], [r'ERROR']),
	('goto-recent-kept', dict(MakeNavigationCopy(), **{
		#What Go to Class and Go to Symbol went to is written to the workspace's user's file, the oldest first, and the
		#commands Go to Action ran to the settings; with nothing typed the Actions tab lists those first, the last run
		#at the top.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5', 'SAVE_LAYOUT': '1', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'edit:gotolist:0:CSquare,edit:gotolist:0:CShape,edit:gotolist:2:Area,edit:gotolist:2:DrawAll,printuserfile,'
		         'edit:gotolist:3:recfil,edit:key:escape,edit:caret:6:8,edit:gotolist:3:typhie,edit:gotolist:3:,printsettings'}),
		[r'^  <RecentClass Key="CSquare"/>$', r'^  <RecentClass Key="CShape"/>$', r'^  <RecentSymbol Key="CCircle::Area\(\)"/>$', r'^  <RecentSymbol Key="DrawAll\(CShape\*, CCircle\*\)"/>$',
		 r'^Go to \[\]: 2\d\d results', r'^  Type Hierarchy  \[Navigate  Ctrl\+H\]  score 1000000$', r'^  Recent Files  \[Navigate  Ctrl\+E\]  score 999999$',
		 r'^  <GoTo>$', r'^    <RecentAction Name="Navigate\.RecentFiles"/>$', r'^    <RecentAction Name="Navigate\.TypeHierarchy"/>$', r'^  </GoTo>$'], [r'ERROR']),
	('goto-recent-read', dict(MakeNavigationCopy('  <RecentClass Key="CGone"/>\n  <RecentClass Key="CCircle"/>\n  <RecentClass Key="CSquare"/>\n  <RecentSymbol Key="DrawAll(CShape*, CCircle*)"/>\n'), **{
		#A workspace's recent classes and symbols are read from its user's file when it opens, and the recent actions
		#from the settings: with nothing typed each tab lists its own first, the last gone to at the top.  A class that
		#isn't there any more puts nothing first.
		'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5', 'SAVE_LAYOUT': '1',
		'SETTINGS': MakeSettingsFile('<Settings>\n  <GoTo>\n    <RecentAction Name="Navigate.RecentLocations"/>\n    <RecentAction Name="No.SuchCommand"/>\n    <RecentAction Name="Navigate.SelectIn"/>\n  </GoTo>\n</Settings>\n'),
		'DEBUG': 'edit:gotolist:0:,edit:gotolist:2:,edit:key:escape,edit:gotolist:3:,edit:key:escape'}),
		[r'^Go to \[\]: 5 results', r'^  CSquare  \[Shapes\.h:17\]  score 1000000$', r'^  CCircle  \[Shapes\.h:6\]  score 999999$',
		 r'^Go to \[\]: 26 results', r'^  DrawAll\(CShape\*, CCircle\*\)  \[Main\.cpp:10\]  score 1000000$',
		 r'^Go to \[\]: 2\d\d results', r'^  Select In  \[Navigate  Alt\+F1\]  score 1000000$', r'^  Recent Locations  \[Navigate  Ctrl\+Shift\+E\]  score 99999\d$'], [r'ERROR', r'CGone']),
	('goto-actions', {
		#Go to Action (Control+Shift+A), the dialog's Actions tab: a command found by its caption, with its menu and its
		#keys after it and nothing to preview; choosing it runs it (Recent Files opens its chooser).  With nothing typed
		#every command is listed.  One that can't be run where the caret is says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '12:5',
		'DEBUG': 'edit:open:Shapes.h,edit:gotolist:3:recfil,edit:printchooser,edit:key:escape,edit:gotolist:3:,edit:caret:6:8,edit:gotolist:3:typhie,edit:printhierarchy,closeall,edit:gotolist:3:quickdef,printstatus'},
		[r'^Go to \[recfil\]: 1 results', r'^  Recent Files  \[Navigate  Ctrl\+E\]', r'^Go to preview: none$', r'^Chooser: Recent Files$', r'^  >Shapes\.cpp \| ',
		 r'^Go to \[\]: 2\d\d results', r'^Go to \[typhie\]: 1 results', r'^  Type Hierarchy  \[Navigate  Ctrl\+H\]', r'^Hierarchy: Type: CCircle \(class hierarchy\)$',
		 r'^Go to \[quickdef\]: 1 results', r'^  Quick Definition  \[View  Ctrl\+Shift\+I\]', r"^Status line: Quick Definition isn.t available here\.$"], [r'ERROR']),
	('results-coloured-rows', {
		#A result's line is coloured as code where its own text says how: keywords, numbers, strings, comments and a
		#directive, round the text found, which keeps the match font; the line's number is dim.
		'SOLUTION': QUICK_INFO, 'WAIT_INDEX': '1', 'OPEN': 'Paint.cpp', 'CARET': '1:1', 'DIALOG_ANSWER': 'Open in Find Window',
		'DEBUG': 'find:0:Paint,answered:1,printfindparts'},
		[r'^    \{d:1: \}\{p:#include\} \{s:"\}\{m:Paint\}\{s:\.h"\}$', r'^    \{d:4: \}\{k:static\} C\{m:Paint\}er gcDefaultPainter;$',
		 r'^    \{d:27: \}\{k:return\} \{m:PAINT\}_MAXIMUM;$', r'^    \{d:43: \}\{k:int\} \{m:Paint\}All\(CPainter\* pcPainter, \{k:int\} iCount\)$',
		 r'^    \{d:51: \}pc\{m:Paint\}er->Rect\(i, Blend\(i, iCount, \{n:0\.5f\}\), \{n:2\}, \{n:2\}, PM_Outline\);$',
		 r'^    \{d:5: \}\{p:#define\} \{m:PAINT\}_MAXIMUM \{n:8\}$', r'^    \{d:15: \}\{k:typedef\} \{k:unsigned\} \{k:int\} \{m:Paint\}Colour;$', r'^    \{d:18: \}\{c://\}\{m:Paint\}\{c:s rectangles\.\}$'], [r'ERROR', r'\{\?:']),
	('results-export-autoscroll', {
		#A results pane's Export copies its rows as they are shown to the clipboard (or saves them to a file).  With
		#Autoscroll from Source the result nearest the active editor's caret in its file is selected as the caret moves
		#(the last at or before its line, else the first after), the editor keeping the keys; the setting is kept.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '1:1', 'DIALOG_ANSWER': 'Open in Find Window|Copy to Clipboard', 'SETTINGS': MakeSettingsFile(),
		'DEBUG': 'find:0:Area,answered:1,printfindtoolbar,findtool:Export...,answered:2,printstatus,edit:printclipboard,edit:printfindselected,findtool:Autoscroll from Source,printfindtoolbar,edit:printfindselected,'
		         'edit:focus:editor,edit:caret:22:1,edit:printfindselected,edit:caret:2:1,edit:printfindselected,edit:open:Main.cpp,edit:caret:26:1,edit:printfindselected,edit:print,printsettings'},
		[r'^Toolbar: Rerun, .*Preview\(on\), Navigate with Single Click, Autoscroll from Source, Export\.\.\., Pin Tab$', r'^Dialog \[Export Results\] answered Copy to Clipboard$',
		 r'^Status line: Copied 14 rows to the clipboard\.$', r'^Clipboard:$', r"^Occurrences of 'Area' in the solution  9 matches in 4 files$", r'^  Main\.cpp  4 matches$',
		 r'^    4: int TotalArea\(CShape\* pcFirst, CShape\* pcSecond\)$', r'^  Shapes\.h  2 matches$', r'^    24: int  Area\(void\) override;$',
		 r'^Find selected: 4: int TotalArea', r'^Toolbar: Rerun, .*Autoscroll from Source\(on\), Export\.\.\., Pin Tab$', r'^Find selected: 10: int CCircle::Area\(void\)$',
		 r'^Find selected: 21: int CSquare::Area\(void\)$', r'^Find selected: 10: int CCircle::Area\(void\)$', r'^Find selected: 25: return TotalArea\(&cCircle, &cSquare\);$',
		 r'Main\.cpp \(focus\) line 26: ', r'<FindInFiles .*AutoscrollFromSource="true"'], [r'ERROR']),
	('hierarchy-calls-via', {
		#The callers of a function that overrides another have after them the callers of what it overrides, each
		#saying which, as a call through the base class may land in it; a caller that is the function itself says
		#recursive.  A call that names its class (CSquare::Draw()) is that class's function's alone: it isn't a caller
		#of what overrides it, and is one of the function it names.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Shapes.cpp', 'CARET': '4:15',
		'DEBUG': 'command:Navigate.CallHierarchy,edit:printhierarchy,edit:hierarchyexpand:CShape::Move,edit:printhierarchy,edit:key:enter,edit:print,'
		         'edit:open:Shapes.cpp,edit:caret:27:25,command:Navigate.CallHierarchy,edit:printhierarchy,edit:open:Shapes.cpp,edit:caret:16:15,command:Navigate.CallHierarchy,edit:printhierarchy,'
		         'edit:open:Shape.cpp,edit:caret:11:15,command:Navigate.CallHierarchy,edit:printhierarchy'},
		[r'^Hierarchy: Calls: CCircle::Draw \(callers\)$', r'^ >CCircle::Draw$', r'^    \+ DrawAll  Main\.cpp:13$', r'^    \+ DrawAll  via CShape::Draw  Main\.cpp:12$',
		 r'^    \+ CShape::Move  via CShape::Draw  Shape\.cpp:20$', r'^ >  CShape::Move  via CShape::Draw  Shape\.cpp:20$', r'^      CCircle::Draw  Shapes\.cpp:6  recursive$',
		 r'Shape\.cpp \(focus\) line 20: \[\tDraw\(\);\]', r'^Hierarchy: Calls: CRoundedSquare::Draw \(callers\)$', r'^ >CRoundedSquare::Draw$',
		 r'^    \+ DrawAll  via CShape::Draw  Main\.cpp:12$', r'^    \+ CShape::Move  via CShape::Draw  Shape\.cpp:20$',
		 r'^Hierarchy: Calls: CSquare::Draw \(callers\)$', r'^    \+ CRoundedSquare::Draw  Shapes\.cpp:29$', r'^    \+ DrawAll  via CShape::Draw  Main\.cpp:12$',
		 r'^Hierarchy: Calls: CShape::Draw \(callers\)$', r'^ >CShape::Draw$', r'^    \+ DrawAll  Main\.cpp:12$', r'^    \+ CShape::Move  Shape\.cpp:20$'], [r'ERROR', r'via CSquare::Draw', r'No hierarchy row']),
	('hierarchy-other-bases', {
		#The class view of Type Hierarchy goes up by each class's first base class.  A class of that chain with more base
		#classes says which after its name (the class asked about, and any over it): a class under it doesn't, and the
		#Supertypes view, which has every base as a row, says none.  Method Hierarchy's rows are the class view's.
		'SOLUTION': BASES, 'WAIT_INDEX': '1', 'OPEN': 'Bases.h', 'CARET': '44:8',
		'DEBUG': 'command:Navigate.TypeHierarchy,edit:printhierarchy,edit:hierarchytool:Supertypes Hierarchy,edit:printhierarchy,'
		         'edit:open:Bases.h,edit:caret:39:8,command:Navigate.TypeHierarchy,edit:printhierarchy,'
		         'edit:open:Bases.cpp,edit:caret:15:14,command:Navigate.MethodHierarchy,edit:printhierarchy'},
		[r'^Hierarchy: Type: CLamp \(class hierarchy\)$', r'^  CCounted  Bases\.h$', r'^    CThing  Bases\.h$', r'^      CWidget  also: CNamed  Bases\.h$', r'^        CDial  Bases\.h$',
		 r'^ >        CLamp  also: CSwitchable, CNamed  Bases\.h$', r'^Hierarchy: Type: CLamp \(supertypes\)$', r'^ >CLamp  Bases\.h$', r'^    CDial  Bases\.h$', r'^      CWidget  Bases\.h$',
		 r'^        CNamed  Bases\.h$', r'^    CSwitchable  Bases\.h$', r'^    CNamed  Bases\.h$',
		 r'^Hierarchy: Type: CDial \(class hierarchy\)$', r'^      CWidget  also: CNamed  Bases\.h$', r'^ >      CDial  Bases\.h$', r'^          CLamp  Bases\.h$',
		 r'^Hierarchy: Method: CLamp::Switch \(class hierarchy\)$', r'^      CWidget  also: CNamed  Bases\.h$', r'^ >        CLamp  \+  also: CSwitchable, CNamed  Bases\.h$'], [r'ERROR']),
	('tool-window-keys', {
		#A tool window's key (Alt+2 Bookmarks ... Alt+9 Find, and Structure's and Hierarchy's): the window is shown, in
		#front of its group's tabs, and has the keys; run again while it has them, it is hidden and the editor has the
		#keys.  A window shown that hasn't the keys is given them, not hidden.  Jump to Last Tool Window shows the one
		#that last had the keys, hidden since or not.  View has each window's keys beside its name.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '6:10',
		'DEBUG': 'edit:printtoolwindows,command:View.Bookmarks,edit:printtoolwindows,command:View.Bookmarks,edit:printtoolwindows,edit:print,'
		         'command:View.Build,command:View.FindUsages,edit:printtoolwindows,command:View.Build,edit:printtoolwindows,'
		         'command:View.Structure,edit:printtoolwindows,command:View.Structure,edit:printtoolwindows,edit:print,command:View.JumpToLastToolWindow,edit:printtoolwindows,'
		         'command:Navigate.TypeHierarchy,edit:printtoolwindows,command:View.Hierarchy,edit:printtoolwindows,command:View.Hierarchy,edit:printtoolwindows,'
		         'command:View.Run,command:View.Debug,command:View.Find,edit:printtoolwindows,printmenus'},
		[r'^Tool windows: Solution Explorer 1 \(front\), Structure 2, Find Usages 1 \(front\), Find 2, Duplicates 3, Bookmarks 4, Hierarchy 5, Build 6, Indexing 7, Problems 8, Run 9, Debug 10, Locals \(hidden\), Call Stack \(hidden\), Autos \(hidden\), Watch 1 \(hidden\), Watch 2 \(hidden\), Watch 3 \(hidden\), Watch 4 \(hidden\), Claude 11$', r'^Last tool window: none$',
		 r'^Tool windows: .*Find Usages 1, .*Bookmarks 4 \(front, keys\), ', r'^Last tool window: Bookmarks$',
		 r'^Tool windows: .*Bookmarks \(hidden\), Hierarchy 4 \(front\), ', r'^Last tool window: Bookmarks$', r'Main\.cpp \(focus\) line 6: ',
		 r'^Tool windows: .*Find Usages 1 \(front, keys\), .*Build 5, ', r'^Tool windows: .*Find Usages 1, .*Build 5 \(front, keys\), ', r'^Last tool window: Build$',
		 r'^Tool windows: Solution Explorer 1, Structure 2 \(front, keys\), ', r'^Tool windows: Solution Explorer \(front\), Structure \(hidden\), ', r'^Last tool window: Structure$', r'Main\.cpp \(focus\) line 6: ',
		 r'^Tool windows: Solution Explorer 1, Structure 2 \(front, keys\), ',
		 r'^Tool windows: .*Hierarchy 4 \(front, keys\), ', r'^Tool windows: .*Hierarchy \(hidden\), ', r'^Tool windows: .*Hierarchy 4 \(front, keys\), ',
		 r'^Tool windows: .*Find 2 \(front, keys\), ', r'^Last tool window: Find$',
		 r'^Menu View: Solution Explorer \[Alt\+1\], Structure \[Alt\+7\], Find Usages \[Alt\+3\], Find \[Alt\+9\], Duplicates, Bookmarks \[Alt\+2\], Hierarchy \[Alt\+8\], Build \[Alt\+6\], Indexing, Problems, Run \[Alt\+4\], Debug \[Alt\+5\], Locals, Call Stack, Autos, Watch 1, Watch 2, Watch 3, Watch 4, Claude, '
		 r'Hide All Tool Windows \[Ctrl\+Shift\+F12\], Jump to Last Tool Window, Quick Definition'], [r'ERROR']),
	('tool-window-real-keys', {
		#The tool windows' keys as a keyboard's presses: Alt+2 from the editor shows Bookmarks with the keys, and Alt+2
		#again, pressed in Bookmarks, hides it; Alt+7 the same for Structure, from its tree; Control+Shift+F12 hides
		#every tool window and, again, puts them back.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '6:10',
		'DEBUG': 'edit:printcrumbs,edit:press:Left Alt,edit:tap:2,edit:release:Left Alt,edit:printtoolwindows,edit:press:Left Alt,edit:tap:2,edit:release:Left Alt,edit:printtoolwindows,edit:print,'
		         'edit:press:Left Alt,edit:tap:7,edit:release:Left Alt,edit:printtoolwindows,edit:press:Left Alt,edit:tap:7,edit:release:Left Alt,edit:printtoolwindows,'
		         'edit:press:Left Control,edit:press:Left Shift,edit:tap:F12,edit:release:Left Shift,edit:release:Left Control,edit:printtoolwindows,'
		         'edit:press:Left Control,edit:press:Left Shift,edit:tap:F12,edit:release:Left Shift,edit:release:Left Control,edit:printtoolwindows,edit:print'},
		[r'^Tool windows: .*Bookmarks 4 \(front, keys\), ', r'^Tool windows: .*Bookmarks \(hidden\), ', r'Main\.cpp \(focus\) line 6: ',
		 r'^Tool windows: Solution Explorer 1, Structure 2 \(front, keys\), ', r'^Tool windows: Solution Explorer \(front\), Structure \(hidden\), ',
		 r'^Tool windows: Solution Explorer \(hidden\), Structure \(hidden\), Find Usages \(hidden\), .*Claude \(hidden\)$',
		 r'^Hidden by Hide All: Solution Explorer, Claude, Debug, Run, Problems, Indexing, Build, Hierarchy, Duplicates, Find, Find Usages$',
		 r'^Tool windows: Solution Explorer \(front\), Structure \(hidden\), Find Usages 1, Find 2, Duplicates 3, Bookmarks \(hidden\), Hierarchy 4 \(front\), Build 5, Indexing 6, Problems 7, Run 8, Debug 9, Locals \(hidden\), Call Stack \(hidden\), Autos \(hidden\), Watch 1 \(hidden\), Watch 2 \(hidden\), Watch 3 \(hidden\), Watch 4 \(hidden\), Claude 10$',
		 r'^Hidden by Hide All: none$', r'Main\.cpp \(focus\) line 6: '], [r'ERROR']),
	('tool-windows-hide-all', {
		#Hide All Tool Windows: every docked tool window is hidden and the editor has the keys; again, they are back
		#where they were, each group as large, its tabs in their order and the same tab in front.  Windows shown by
		#themselves in between (and hidden by their keys or by the command) don't take the others' place: what was
		#hidden is kept until it is put back, and a window goes back to where it was the first time.  With nothing
		#docked and nothing hidden by it, the status line says so.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '6:10',
		'DEBUG': 'command:View.Structure,command:View.Build,edit:printtoolwindows,command:View.HideAllToolWindows,edit:printtoolwindows,edit:print,'
		         'command:View.HideAllToolWindows,edit:printtoolwindows,command:View.HideAllToolWindows,command:View.Find,edit:printtoolwindows,command:View.Find,'
		         'command:View.Run,command:View.Bookmarks,edit:printtoolwindows,command:View.HideAllToolWindows,edit:printtoolwindows,command:View.HideAllToolWindows,edit:printtoolwindows'},
		[r'^Tool windows: Solution Explorer 1, Structure 2 \(front\), Find Usages 1, Find 2, Duplicates 3, Bookmarks 4, Hierarchy 5, Build 6 \(front, keys\), Indexing 7, Problems 8, Run 9, Debug 10, Locals \(hidden\), Call Stack \(hidden\), Autos \(hidden\), Watch 1 \(hidden\), Watch 2 \(hidden\), Watch 3 \(hidden\), Watch 4 \(hidden\), Claude 11$',
		 r'^Tool window areas: Structure 5,72 320x699, Build 330,591 945x180$', r'^Hidden by Hide All: none$',
		 r'^Tool windows: Solution Explorer \(hidden\), Structure \(hidden\), Find Usages \(hidden\), Find \(hidden\), Duplicates \(hidden\), Bookmarks \(hidden\), Hierarchy \(hidden\), Build \(hidden\), Indexing \(hidden\), Problems \(hidden\), Run \(hidden\), Debug \(hidden\), Locals \(hidden\), Call Stack \(hidden\), Autos \(hidden\), Watch 1 \(hidden\), Watch 2 \(hidden\), Watch 3 \(hidden\), Watch 4 \(hidden\), Claude \(hidden\)$',
		 r'^Tool window areas: none$', r'^Last tool window: Build$', r'^Hidden by Hide All: Structure, Solution Explorer, Claude, Debug, Run, Problems, Indexing, Build, Hierarchy, Bookmarks, Duplicates, Find, Find Usages$',
		 r'Main\.cpp \(focus\) line 6: ',
		 r'^Tool windows: Solution Explorer 1, Structure 2 \(front\), Find Usages 1, Find 2, Duplicates 3, Bookmarks 4, Hierarchy 5, Build 6 \(front\), Indexing 7, Problems 8, Run 9, Debug 10, Locals \(hidden\), Call Stack \(hidden\), Autos \(hidden\), Watch 1 \(hidden\), Watch 2 \(hidden\), Watch 3 \(hidden\), Watch 4 \(hidden\), Claude 11$',
		 r'^Tool window areas: Structure 5,72 320x699, Build 330,591 945x180$', r'^Hidden by Hide All: none$',
		 r'^Tool windows: Solution Explorer \(hidden\), .*Find \(front, keys\), Duplicates \(hidden\), ', r'^Tool window areas: Find 5,591 1270x180$',
		 r'^Tool windows: .*Find \(hidden\), Duplicates \(hidden\), Bookmarks 2 \(front, keys\), Hierarchy \(hidden\), Build \(hidden\), Indexing \(hidden\), Problems \(hidden\), Run 1, ',
		 r'^Hidden by Hide All: Structure, Solution Explorer, Claude, Debug, Run, Problems, Indexing, Build, Hierarchy, Bookmarks, Duplicates, Find, Find Usages$',
		 r'^Tool window areas: none$', r'^Hidden by Hide All: Structure, Solution Explorer, Claude, Debug, Run, Problems, Indexing, Build, Hierarchy, Bookmarks, Duplicates, Find, Find Usages$',
		 r'^Tool windows: Solution Explorer 1, Structure 2 \(front\), Find Usages 1, Find 2, Duplicates 3, Bookmarks 4, Hierarchy 5, Build 6 \(front\), Indexing 7, Problems 8, Run 9, Debug 10, Locals \(hidden\), Call Stack \(hidden\), Autos \(hidden\), Watch 1 \(hidden\), Watch 2 \(hidden\), Watch 3 \(hidden\), Watch 4 \(hidden\), Claude 11$',
		 r'^Tool window areas: Structure 5,72 320x699, Build 330,591 945x180$', r'^Last tool window: Bookmarks$', r'^Hidden by Hide All: none$'], [r'ERROR']),
	('tool-windows-jump-to-last', {
		#Jump to Last Tool Window does nothing until a tool window has had the keys.  Then it shows the last that had
		#them and gives them back: one hidden by its own key, and one hidden with the rest by Hide All Tool Windows,
		#which comes back by itself.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '6:10',
		'DEBUG': 'command:View.JumpToLastToolWindow,edit:printtoolwindows,command:View.Bookmarks,command:View.Build,command:View.Build,edit:printtoolwindows,edit:print,'
		         'command:View.JumpToLastToolWindow,edit:printtoolwindows,command:View.HideAllToolWindows,edit:printtoolwindows,command:View.JumpToLastToolWindow,edit:printtoolwindows'},
		[r'^Command \[View\.JumpToLastToolWindow\] not found, not enabled or did nothing\.$', r'^Last tool window: none$',
		 r'^Tool windows: .*Bookmarks 4, Hierarchy 5, Build \(hidden\), Indexing 6 \(front\), Problems 7, ', r'^Last tool window: Build$', r'Main\.cpp \(focus\) line 6: ',
		 r'^Tool windows: .*Bookmarks 4, Hierarchy 5, Build 6 \(front, keys\), ', r'^Tool windows: .*Build \(hidden\), ', r'^Last tool window: Build$',
		 r'^Tool windows: .*Hierarchy \(hidden\), Build \(front, keys\), Indexing \(hidden\), Problems \(hidden\), ', r'^Tool window areas: Build 5,591 1270x180$'], [r'ERROR']),
	('macro-define-link', dict(MakeMacrosCopy(), **{
		#A macro's name is no symbol of the index's, and its #define is its declaration: with Control held over it the
		#name is a link, the pointer a hand, and the #define's first line is shown with where it is; Quick Definition
		#previews it; Go to Source, Go to Definition and Control+click go to it.  A variable's name is still the
		#index's, and a name that is neither does what it did.
		'WAIT_INDEX': '1', 'OPEN': 'Use.cpp', 'CARET': '21:20',
		'DEBUG': 'edit:printcrumbs,edit:linkhover:21:18,edit:printinfo,edit:linkhover:off,edit:caret:29:10,edit:linkhover:29:10,edit:printinfo,edit:linkhover:off,edit:linkhover:27:3,edit:printinfo,edit:linkhover:off,'
		         'command:View.QuickDefinition,edit:printchooser,edit:key:escape,command:Navigate.GoToSource,edit:print,edit:open:Use.cpp,edit:caret:28:14,command:Navigate.GoToDefinition,edit:print,'
		         'edit:open:Use.cpp,edit:caret:21:3,edit:controlclick:21:20,edit:print,edit:open:Use.cpp,edit:caret:19:2,command:Navigate.GoToSource,command:View.QuickDefinition,printstatus'}),
		[r'^Link: \[MAX_UNITS\] #define MAX_UNITS 8 \| Macros\.h:5$', r'^Pointer: hand',
		 r'^Link: \[BIGGER\] #define BIGGER\(a, b\) \(\(a\) > \(b\) \? \(a\) : \(b\)\) \| Macros\.h:16$', r'^Link: \[fRate\] float fRate \| Use\.cpp:24$',
		 r'^Chooser: Definition of BIGGER$', r'^  >BIGGER \| Macros\.h:16$', r'^Chooser preview: Macros\.h$',
		 r'Macros\.h \(focus\) line 16: \[#define BIGGER\(a, b\)', r'Macros\.h \(focus\) line 7: \[#define TITLE\t\t"Units"\]',
		 r'Macros\.h \(focus\) line 5: \[#define MAX_UNITS\t8', r'^Command \[Navigate\.GoToSource\] not found, not enabled or did nothing\.$',
		 r'^Status line: There is no symbol at the caret to show the definition of\.$'], [r'ERROR']),
	('type-of-expression', {
		#Where the caret is on no symbol with a type, Type Info is the type of the expression it is in: the & of an
		#address, an operator, a call's bracket, a number.  A name's is still its declared type, and outside any
		#expression there is none.  Go to Type Declaration goes to that type's class, says so when the type has no
		#declaration (int), and says there is no symbol where there is no expression either.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '24:10',
		'DEBUG': 'command:View.TypeInfo,edit:printinfo,edit:caret:6:25,command:View.TypeInfo,edit:printinfo,edit:caret:6:22,command:View.TypeInfo,edit:printinfo,'
		         'edit:caret:22:15,command:View.TypeInfo,edit:printinfo,edit:caret:6:10,command:View.TypeInfo,edit:printinfo,edit:caret:18:1,command:View.TypeInfo,printstatus,'
		         'edit:caret:24:10,command:Navigate.GoToTypeDeclaration,edit:print,edit:open:Main.cpp,edit:caret:25:29,command:Navigate.GoToTypeDeclaration,edit:print,'
		         'edit:open:Main.cpp,edit:caret:6:25,command:Navigate.GoToTypeDeclaration,printstatus,edit:caret:18:1,command:Navigate.GoToTypeDeclaration,printstatus'},
		[r'^Info: type$', r'^  CCircle\*$', r'^Info: type$', r'^  int$', r'^Info: type$', r'^  int$', r'^Info: type$', r'^  int$', r'^Info: type$', r'^  CShape\*$',
		 r'^Status line: No type is known for what is at the caret\.$',
		 r'Shapes\.h \(focus\) line 6: \[class CCircle : public CShape\]', r'Shapes\.h \(focus\) line 28: \[class CRoundedSquare : public CSquare\]',
		 r'^Status line: The type of the expression at the caret, int, has no declaration to go to\.$',
		 r'^Status line: There is no symbol at the caret to go to the type of\.$'], [r'ERROR']),
	('nav-type-declaration', {
		#Go to Type Declaration (Control+Shift+B): from a parameter and from a local variable, the class each is declared
		#with; from a field of a built in type, the status line says its type has nothing to go to.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '6:10',
		'DEBUG': 'command:Navigate.GoToTypeDeclaration,edit:print,edit:open:Main.cpp,edit:caret:23:3,command:Navigate.GoToTypeDeclaration,edit:print,'
		         'edit:open:Shapes.cpp,edit:caret:12:14,command:Navigate.GoToTypeDeclaration,printstatus'},
		[r'Shape\.h \(focus\) line 6: \[class CShape\]', r'Shapes\.h \(focus\) line 28: \[class CRoundedSquare : public CSquare\]',
		 r'^Status: The type of CCircle::miRadius has no declaration to go to\.$'], []),
	('nav-type-declaration-second', {
		#The second of two parameters has its own type, not the first's; a class is its own type.
		'SOLUTION': NAVIGATION, 'WAIT_INDEX': '1', 'OPEN': 'Main.cpp', 'CARET': '13:5',
		'DEBUG': 'command:Navigate.GoToTypeDeclaration,edit:print,edit:open:Main.cpp,edit:caret:20:5,command:Navigate.GoToTypeDeclaration,edit:print'},
		[r'Shapes\.h \(focus\) line 6: \[class CCircle : public CShape\]', r'Shapes\.h \(focus\) line 28: \[class CRoundedSquare : public CSquare\]'], []),
]

if os.name != 'nt':
	#The stand-in for Claude Code is a shell script, which Windows can't start as a program.
	CHECKS.append(('claude-request', {
		#The Claude tab used as it is by hand, with a stand-in for Claude Code (Input/Claude/claude) that reads Engine.h
		#and answers with what it was asked.  A press in the request's empty box gives it the keys (sized by its text it
		#was no pixels wide, the press missed it and nothing could be typed), what is typed goes in it, and a press on
		#Send sends it: the request, the tool's line (the file named from Claude's folder) and the answer are in the
		#conversation, the box is empty and has the keys again, so the next request is typed without another press.
		#Shift+Enter, pressed on the script's keyboard, starts a line in the request and Enter sends it, both lines.
		#Claude Code is started with CLAUDE_CODE_ADDITIONAL_DIRECTORIES_CLAUDE_MD=1, so the CLAUDE.md files of the
		#folders it is also given are read.
		'SOLUTION': RENAME_FILE, 'OPEN': 'Engine.h', 'CLAUDE_EXECUTABLE': os.path.join(HERE, 'Input', 'Claude', 'claude'),
		'DEBUG': 'wait:500,clickmenu:View/Claude,edit:claudeclick:request,edit:printclaude,edit:claudetype:Hello there,edit:printclaude,'
		         'edit:claudeclick:send,wait:1500,edit:printclaude,edit:claudetype:Second,edit:press:Left Shift,edit:tap:Enter,'
		         'edit:release:Left Shift,edit:claudetype:line,edit:printclaude,edit:tap:Enter,wait:1500,edit:printclaude'},
		[r'^Claude: .* keys request$', r'^Claude request: \[\]$', r'^Claude request: \[Hello there\]$',
		 r'^Claude: .* keys request$', r'^Claude status: \[Done, \$0\.01\.\]$', r'^Claude request: \[\]$',
		 r'^Claude conversation: \[> Hello there$', r'^  Read Engine\.h$', r'^Started in RenameFile \(new\), also given: HCP65-IDE$',
		 r'^Their CLAUDE\.md files: read$', r'^Asked: Hello there\]$', r'^Claude request: \[Second$', r'^line\]$',
		 r'^Claude: .* keys request$', r'^Claude request: \[\]$', r'^> Second$', r'^> line$',
		 r'^Started in RenameFile \(carried on\), also given: HCP65-IDE$', r'^Asked: Second\]$'],
		[r'^Claude: .* keys nothing$']))
	CHECKS.append(('claude-directory-repository', {
		#Settings' Claude page's "The startup solution's repository": Claude is started in the repository the startup
		#solution's folder is in, which every project of this solution is in too, so it is given no other folder.
		'SOLUTION': RENAME_FILE, 'OPEN': 'Engine.h', 'CLAUDE_EXECUTABLE': os.path.join(HERE, 'Input', 'Claude', 'claude'),
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <Claude Directory="Repository" Folder=""/>\n</Settings>\n'),
		'DEBUG': 'wait:500,clickmenu:View/Claude,edit:claudeclick:request,edit:claudetype:Where,edit:tap:Enter,wait:1500,edit:printclaude'},
		[r'^Claude status: \[Done, \$0\.01\.\]$', r'^Claude conversation: \[> Where$', r'^Started in HCP65-IDE \(new\), also given:$', r'^Asked: Where\]$'],
		[]))
	CHECKS.append(('claude-directory-setting', {
		#Settings' Claude page: a request is sent from the startup solution's folder, the page's "The folder below" is
		#chosen with Sub typed (a folder of the solution's, as it isn't a full path) and kept by OK, and the next request
		#is started in Sub as a new conversation, which the conversation says: Claude Code keeps a conversation with the
		#folder it was started in.  The setting is written with the settings.
		'SOLUTION': RENAME_FILE, 'OPEN': 'Engine.h', 'CLAUDE_EXECUTABLE': os.path.join(HERE, 'Input', 'Claude', 'claude'),
		'SETTINGS': MakeSettingsFile(), 'CLAUDE_EDITS': 'directory:Folder;folder:Sub', 'DIALOG_ANSWER': 'OK',
		'DEBUG': 'wait:500,clickmenu:View/Claude,edit:claudeclick:request,edit:claudetype:One,edit:tap:Enter,wait:1500,'
		         'clickmenu:File/Settings...,answered:1,printsettings,edit:claudeclick:request,edit:claudetype:Two,edit:tap:Enter,wait:1500,edit:printclaude'},
		[r'^Dialog \[Settings\] answered OK$', r'<Claude Directory="Folder" Folder="Sub"/>', r'^Claude conversation: \[> One$',
		 r'^Started in RenameFile \(new\), also given: HCP65-IDE$', r"^Claude's folder changed: a new conversation\.$", r'^> Two$',
		 r'^Started in Sub \(new\), also given: HCP65-IDE$', r'^Asked: Two\]$'],
		[]))
	CHECKS.append(('claude-directory-missing', {
		#A folder of the settings' that isn't there: Claude isn't asked, and the status says where the folder is set.
		'SOLUTION': RENAME_FILE, 'OPEN': 'Engine.h', 'CLAUDE_EXECUTABLE': os.path.join(HERE, 'Input', 'Claude', 'claude'),
		'SETTINGS': MakeSettingsFile('<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n  <Claude Directory="Folder" Folder="Nowhere"/>\n</Settings>\n'),
		'DEBUG': 'wait:500,clickmenu:View/Claude,edit:claudeclick:request,edit:claudetype:Where,edit:tap:Enter,wait:500,edit:printclaude'},
		[r"^Claude status: \[Claude wasn't asked: its folder \[.*RenameFile/Nowhere\] isn't there\.  It is set in File, Settings, Claude\.\]$",
		 r'^Claude request: \[Where\]$', r'^Claude conversation: \[\]$'],
		[]))


def Hashes():
	asHashes = {}
	for szFile in WATCHED:
		if os.path.exists(szFile):
			asHashes[szFile] = hashlib.md5(open(szFile, 'rb').read()).hexdigest()
	return asHashes


def Run(szExecutable, szName, asVariables):
	asEnv = dict(os.environ)
	#The IDE sets it for Claude Code itself (claude-request); one inherited from here would hide that it didn't.
	asEnv.pop('CLAUDE_CODE_ADDITIONAL_DIRECTORIES_CLAUDE_MD', None)
	szSnapshot = os.path.join(tempfile.gettempdir(), 'IDEChecks-%d.png' % os.getpid())
	asEnv['HCP65_IDE_SNAPSHOT'] = szSnapshot
	asEnv['HCP65_IDE_SNAPSHOT_TICKS'] = '3'
	if CHECKS_RECENT and ('RECENT' not in asVariables):
		asEnv['HCP65_IDE_RECENT'] = CHECKS_RECENT
	aszArguments = []
	for szKey, szValue in asVariables.items():
		if szKey == 'PREPARE':
			#What the run needs made first, not a variable.
			szValue()
		elif szKey == 'SOLUTION':
			aszArguments = [szValue]
		elif szValue is None:
			asEnv.pop('HCP65_IDE_' + szKey, None)
		else:
			asEnv['HCP65_IDE_' + szKey] = szValue
	cResult = subprocess.run([szExecutable] + aszArguments, cwd=IDE_DIR, env=asEnv, capture_output=True, timeout=300)
	if os.path.exists(szSnapshot):
		os.remove(szSnapshot)
	szOut = cResult.stdout.decode('latin-1') + cResult.stderr.decode('latin-1')
	#The indexer's threads print each file they process (CProject::DumpProcessedCPPFileName), which can land in the
	#middle of a line the script prints: taking it out wherever it is joins that line up again.
	szOut = re.sub(r'Process \([^)\n]*\)\n', '', szOut)
	return cResult.returncode, szOut.split('\n')


def Check(szExecutable, szName, asVariables, aszExpected, aszForbidden, szOutputSuffix=''):
	asBefore = Hashes()
	bBreakpoints = os.path.exists(BREAKPOINTS)
	iCode, aszLines = Run(szExecutable, szName, asVariables)
	if OUTPUTS:
		open(os.path.join(OUTPUTS, szName + szOutputSuffix + '.txt'), 'w').write('\n'.join(aszLines))
	aszProblems = []
	if iCode != 0:
		aszProblems.append('exited with %d' % iCode)
	iLine = 0
	for szExpected in aszExpected:
		while (iLine < len(aszLines)) and not re.search(szExpected, aszLines[iLine]):
			iLine += 1
		if iLine == len(aszLines):
			aszProblems.append('no line (in order) matching: ' + szExpected)
			break
		iLine += 1
	if os.environ.get('HCP65_IDE_VERIFY_REDRAW'):
		#Every partial paint is checked against drawing all of the window; one that differed is logged.
		aszForbidden = aszForbidden + [r'A partial paint of']
	if os.environ.get('HCP65_IDE_VERIFY_PRESENT'):
		#What a window shows after each present is checked against what it drew; one that differed is logged.
		aszForbidden = aszForbidden + [r'A present of']
	if os.environ.get('HCP65_IDE_VERIFY_LAYOUT'):
		#The window is laid out every paint though nothing asked for it; a component that moved is logged.
		aszForbidden = aszForbidden + [r"but the layout wasn't invalidated"]
	for szForbidden in aszForbidden:
		for szLine in aszLines:
			if re.search(szForbidden, szLine):
				aszProblems.append('line matching %s: %s' % (szForbidden, szLine))
	asAfter = Hashes()
	for szFile in WATCHED:
		if asBefore.get(szFile) != asAfter.get(szFile):
			aszProblems.append('changed: ' + szFile)
	if not bBreakpoints and os.path.exists(BREAKPOINTS):
		os.remove(BREAKPOINTS)
	if aszProblems:
		print('FAIL %s' % szName)
		for szProblem in aszProblems:
			print('    ' + szProblem)
		print('    output:')
		for szLine in aszLines[-40:]:
			print('    | ' + szLine)
		return False
	print('pass %s' % szName)
	return True


def TakeOption(aszArgs, szOption, bValue):
	#Whether szOption is in aszArgs, or with bValue the argument after it (None if it isn't there); both taken out.
	if szOption not in aszArgs:
		return None if bValue else False
	i = aszArgs.index(szOption)
	if not bValue:
		del aszArgs[i]
		return True
	if i + 1 >= len(aszArgs):
		sys.exit('IDEChecks: %s needs an argument.' % szOption)
	szValue = aszArgs[i + 1]
	del aszArgs[i:i + 2]
	return szValue


def RunChecks(szExecutable, aszNames, szOutputSuffix=''):
	#The checks named (all of them if none are) on szExecutable.  Returns how many failed.
	iFailed = 0
	for szName, asVariables, aszExpected, aszForbidden in CHECKS:
		if aszNames and szName not in aszNames:
			continue
		if not Check(szExecutable, szName, asVariables, aszExpected, aszForbidden, szOutputSuffix):
			iFailed += 1
	return iFailed


def main():
	global OUTPUTS
	aszArgs = sys.argv[1:]
	szGiven = None
	if aszArgs and os.path.isfile(aszArgs[0]):
		szGiven = os.path.abspath(aszArgs.pop(0))
	bNative = TakeOption(aszArgs, '-native', False)
	OUTPUTS = TakeOption(aszArgs, '-outputs', True)
	if OUTPUTS:
		os.makedirs(OUTPUTS, exist_ok=True)
	aszOpen = []
	while '-open' in aszArgs:
		aszOpen.append(TakeOption(aszArgs, '-open', True))
	OpenForChecks(aszOpen if aszOpen else [os.path.join(MODPLAY, 'HCP65ModPlay.hcp65workspace')])
	atexit.register(RestoreUserFiles, {szFile: (open(szFile, 'rb').read() if os.path.exists(szFile) else None) for szFile in RESTORED})

	if szGiven:
		iFailed = RunChecks(szGiven, aszArgs)
	elif bNative:
		iFailed = RunChecks(NATIVE_EXECUTABLE, aszArgs)
	else:
		iFailed = RunChecks(REFERENCE_EXECUTABLE, aszArgs)
		if not aszArgs:
			print('Native smoke checks on %s' % NATIVE_EXECUTABLE)
			iFailed += RunChecks(NATIVE_EXECUTABLE, NATIVE_SMOKE, '.native')
	print('%d failed' % iFailed)
	return 1 if iFailed else 0


if __name__ == '__main__':
	sys.exit(main())
