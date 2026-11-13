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


def MakeBrokenKernel():
	#A copy of the test solution whose kernel doesn't compile: two errors in Kernel.cpp, on lines 2 and 5, and a usage of
	#F on line 5.  Its recent solutions are apart from the user's.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szKernel = os.path.join(szRoot, 'Workspace', 'Kernel')
	shutil.copytree(os.path.join(ROOT, 'HCP65-IDE', 'Tests', 'TestBuildLib', 'TestBuildLib', 'Input', 'Workspace'), os.path.join(szRoot, 'Workspace'))
	open(os.path.join(szKernel, 'Kernel.cpp'), 'w', newline='').write('int main(void) { return 0; }\nint F(void) { return giMissing; }\nint G(void)\n{\n\treturn F() + giAlsoMissing;\n}\n')
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


def MakeRenameFileCopy():
	#A copy of Rename File's solution (Input/RenameFile), which a check can rename files in.  Its recent solutions are
	#apart from the user's.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szFolder = os.path.join(szRoot, 'RenameFile')
	shutil.copytree(os.path.join(HERE, 'Input', 'RenameFile'), szFolder)
	return {'SOLUTION': os.path.join(szFolder, 'RenameFile.sln'), 'RECENT': os.path.join(szRoot, 'Recent.xml')}


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
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usages,references,renamable'},
		[r'^Usages of IsParentheses and 1 override  3 usages$',
		 r'^  Call  3 usages$', r'^    BaseLib  3 usages$', r'^      Calculator\.cpp  3$', r'^        CCalculator  3$', r'^          367: ',
		 r'^          391: ', r'^          421: ',
		 r'^References to IsParentheses and 1 override  7 references$',
		 r'^  Definition  2 references$', r'^  Declaration  2 references$', r'^      CalcParentheses\.h  1$', r'^          38: ',
		 r'^  Call  3 references$',
		 r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\)\] at 7 places, 1 overrides$'], [r'\[declaration\]']),
	('usages-files-only', {
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,references,usagegroups:c,usages'},
		[r'^References to IsParentheses and 1 override  7 references$',
		 r'^  CalcExpression\.cpp  1$', r'^    60: .*\[definition\]$', r'^  CalcExpression\.h  1$', r'^    37: .*\[declaration\]$',
		 r'^  CalcParentheses\.cpp  1$', r'^  CalcParentheses\.h  1$', r'^  Calculator\.cpp  3$', r'^    367: ', r'^    391: ', r'^    421: ',
		 r'^Usages of IsParentheses and 1 override  3 usages$', r'^  Calculator\.cpp  3$', r'^    CCalculator  3$', r'^      367: '],
		[r'BaseLib  ', r'Call  ']),
	('usages-enter', {
		#Enter in the Find Usages tree goes to the selected usage, as double clicking it does: the second of Calculator.cpp's.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:enter'},
		[r'^  Calculator\.cpp  3$', r'^    391: ', r'Calculator\.cpp \(focus\) line 391: '], []),
	('usages-jump-to-declaration', {
		#F4 in the Find Usages tree goes to the symbol's definition, as IntelliJ's goes to the declaration, whichever usage
		#is selected.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:f4'},
		[r'^    391: ', r'CalcParentheses\.cpp \(focus\) line 72: '], [r'Calculator\.cpp \(focus\)']),
	('usages-first-selected', {
		#Find Usages selects the first usage and gives the tree the keys, for enter to go to it; grouped again, the usage
		#selected stays selected.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,printselected,select:3,usagegroups:c,printselected,usagegroups:,usages,key:enter'},
		[r'^Selected: 367: ', r'^Selected: 391: ', r'Calculator\.cpp \(focus\) line 367: '], [r'^Selected: none']),
	('usages-next-occurrence', {
		#Control+Alt+Down and Up in the Find Usages tree go to the next and previous usage, each opened in its editor with
		#the tree keeping the keys; past the last nothing moves.  (The editor opened has the keys by the time the script
		#runs, so the tree is given them again.)
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,command:Navigate.NextOccurrence,edit:printselected,edit:printselection,command:Navigate.NextOccurrence,edit:printselected,'
		         'command:Navigate.NextOccurrence,printstatus,edit:printselected,command:Navigate.PreviousOccurrence,edit:printselected,edit:print'},
		[r'^Selected: 391: ', r'^Selection: \[IsParentheses\] on line 391, caret line 391$', r'^Selected: 421: ',
		 r'^Status line: No usages after this one\.$', r'^Selected: 421: ', r'^Selected: 391: ', r'Calculator\.cpp line 391: '],
		[r'not enabled', r'Calculator\.cpp \(focus\)']),
	('usages-step-from-editor', {
		#From a source editor, Control+Alt+Down steps through Find Usages, the one tool window seen with something to step
		#to, from the usage selected; the editor opened has the keys, so it goes on from there.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
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
	('steps-ambiguous', dict(MakeBrokenKernel(), **{
		#Find Usages floated beside the Build tab, both seen with something to step to: from the editor neither steps.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '2:6', 'BUILD': '1', 'DOCK': 'float:FindUsages', 'EDIT': 'usages',
		'DEBUG': 'wait:6000,edit:focus:editor,command:Navigate.NextOccurrence,printstatus,edit:print'}),
		[r'^Usages of F  1 usage$', r'^Status line: Find Usages and Build can both step: click in the one to step through\.$',
		 r'Kernel\.cpp \(focus\) line 2: '], [r'Kernel\.cpp \(focus\) line 5: ']),
	('nav-back-forward', {
		#A jump (enter in Find Usages) puts where it left on Back; Back goes there and puts where it was on Forward; Forward
		#goes back again.  (The tabs the user's layout opens can be on Back before them.)
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,edit:printhistory,command:Navigate.Back,edit:printhistory,edit:print,'
		         'command:Navigate.Forward,edit:printhistory,edit:print'},
		[r'^History: back \[(.*, )?CalcParentheses\.h:38\] forward \[\]$', r'^History: back \[.*\] forward \[Calculator\.cpp:367\]$',
		 r'CalcParentheses\.h \(focus\) line 38: ', r'^History: back \[(.*, )?CalcParentheses\.h:38\] forward \[\]$',
		 r'Calculator\.cpp \(focus\) line 367: '], [r'not enabled']),
	('nav-steps', {
		#Each Next Occurrence step is a place: Back goes back through them one at a time.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:editor,command:Navigate.NextOccurrence,command:Navigate.NextOccurrence,edit:printhistory,'
		         'command:Navigate.Back,edit:print,command:Navigate.Back,edit:print'},
		[r'^History: back \[(.*, )?CalcParentheses\.h:38, Calculator\.cpp:391\] forward \[\]$', r'Calculator\.cpp \(focus\) line 391: ',
		 r'CalcParentheses\.h \(focus\) line 38: '], [r'not enabled']),
	('nav-closed-file', {
		#Back to a place whose file has been closed opens it again, its caret there.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,closeall,command:Navigate.Back,edit:print'},
		[r'CalcParentheses\.h \(focus\) line 38: '], [r'not enabled', r'Calculator\.cpp.* line \d+: \[']),
	('nav-jump-clears-forward', {
		#A new jump after Back clears Forward, as a browser's does.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
		'DEBUG': 'edit:focus:usages,edit:key:enter,command:Navigate.Back,edit:printhistory,edit:focus:usages,edit:select:3,edit:key:enter,'
		         'edit:printhistory,command:Navigate.Forward'},
		[r'^History: back \[.*\] forward \[Calculator\.cpp:367\]$', r'^History: back \[(.*, )?CalcParentheses\.h:38\] forward \[\]$',
		 r'^Command \[Navigate\.Forward\] not found, not enabled or did nothing\.$'], []),
	('nav-edit-follows', {
		#A place in an open file moves with its line: two lines put in above it move it down two.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages',
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
		#Control+B on a symbol's own declaration shows a popup of its usages, as IntelliJ's, the first selected; Enter
		#goes to it and the popup closes.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'DEBUG': 'gotosource,edit:printpopup,edit:key:enter,edit:printpopup,edit:print'},
		[r'^Popup: Usages of IsParentheses and 1 override  3 usages \| Calculator\.cpp:367  if \(\(\*ppcExpression\)->IsParentheses\(\)\) \| '
		 r'Calculator\.cpp:391  if \(pcExpression->IsParentheses\(\)\) \| Calculator\.cpp:421  if \(pcExpression->IsParentheses\(\)\)$',
		 r'^Popup: none$', r'Calculator\.cpp \(focus\) line 367: '], []),
	('gotosource-popup-scrolls', {
		#CCodabase's many usages: the popup is no taller than a third of the screen or the editor's view, and scrolls.
		'WAIT_INDEX': '1', 'OPEN': 'Codabase.h', 'CARET': '27:8',
		'DEBUG': 'gotosource,edit:printpopup'},
		[r'^Popup: Usages of CCodabase  \d\d+ usages \| ', r'^Popup height: \d+ scrolling, the menu \d+$'], [r'all shown']),
	('gotosource-on-usage', {
		#Control+B on a usage still goes to the definition (CCalcExpression's, as pcExpression is one), with no popup.
		'WAIT_INDEX': '1', 'OPEN': 'Calculator.cpp', 'CARET': '391:22',
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
	('usages-many-files-closed', dict(MakeUsagesKernel(USAGES_KERNEL, {'Aaa.cpp': ManyUsages('A', 600), 'Bbb.cpp': ManyUsages('B', 600)}), **{
		#Past 1000 usages (1204 here) the tree's files start closed, but for the first line's: the top, the three files
		#and Aaa.cpp's 600 lines can be seen.  A step from Aaa.cpp's last line opens Bbb.cpp for its first.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5', 'EDIT': 'usagegroups:,usages,printrows,printselected,select:601',
		'DEBUG': 'edit:focus:usages,command:Navigate.NextOccurrence,edit:printselected,edit:printrows'}),
		[r'^Usages of F  1204 usages$', r'^  Aaa\.cpp  600$', r'^  Bbb\.cpp  600$', r'^  Kernel\.cpp  4$', r'^Rows: 604$',
		 r'^Selected: 2: int A0\(void\)', r'^Selected: 2: int B0\(void\)', r'^Rows: 1204$'], [r'not enabled']),
	('gotosource-popup-more', dict(MakeUsagesKernel(USAGES_KERNEL, {'Aaa.cpp': ManyUsages('A', 150)}), **{
		#Control+B's popup shows 100 usages; its last row says how many more there are, and opens them all in Find
		#Usages.
		'WAIT_INDEX': '1', 'OPEN': 'Kernel.cpp', 'CARET': '1:5',
		'DEBUG': 'gotosource,edit:printpopup,edit:key:documentend,edit:key:enter,edit:printpopup,edit:printusages'}),
		[r'^Popup: Usages of F  154 usages \| Aaa\.cpp:2  int A0\(void\).* \| Aaa\.cpp:101  int A99\(void\) \{ return F\(\); \} \| '
		 r'\.\.\. and 54 more: show them all in Find Usages$', r'^Popup: none$', r'^Usages of F  154 usages$'], [r'A100\(void\).* \| ']),
	('usages-keys', {
		#Find Usages' keys, and Jump to Source's, are bound: F4, Alt+F7 and Control+Alt+Down and Up.
		'DEBUG': 'printkeys'},
		[r'^Keys: Window\.JumpToSource F4 bound$', r'^Keys: Navigate\.FindUsages Alt\+F7 bound$',
		 r'^Keys: Navigate\.NextOccurrence Control\+Alt\+Cursor Down bound$', r'^Keys: Navigate\.PreviousOccurrence Control\+Alt\+Cursor Up bound$',
		 r'^Keys: Navigate\.Back Control\+Alt\+Cursor Left bound$', r'^Keys: Navigate\.Forward Control\+Alt\+Cursor Right bound$'],
		[r'NOT BOUND']),
	('usages-typing', {
		#Space, and a character typed, in the Find Usages tree are typed in the active editor, which gets the keys: no
		#usage is gone to.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:space,key:type:ab'},
		[r'^    391: ', r'CalcParentheses\.h.*\(focus\) line 38: \[.* ab'], [r'Calculator\.cpp.* line \d+: \[']),
	('usages-single-click', {
		#With Navigate with Single Click a usage is opened as it is selected, and the tree keeps the keys; without, selecting
		#one opens nothing.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,focus:usages,select:3,print,usagesingleclick:on,select:4,print'},
		[r'CalcParentheses\.h line 38: ', r'Calculator\.cpp line 421: '], [r'line 391: \[', r'Calculator\.cpp \(focus\)']),
	('find-jump-to-source', {
		#The Find tool window's results are of nothing declared: F4 opens the selected one, as enter does.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,edit:key:f4,edit:print'},
		[r'CalculatorParser\.h \(focus\) line 29: '], [r'ERROR']),
	('usages-escape', {
		#Escape in the Find Usages tree gives the keys back to the editor, where they were.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usages,focus:usages,select:3,key:escape'},
		[r'^    391: ', r'CalcParentheses\.h \(focus\) line 38: '], [r'Calculator\.cpp.* line \d+: \[']),
	('rename-after-edits', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2',
		'EDIT': 'type:int i; ,rename:mpcCalc2,rename:mpcCalc3,renamable'},
		[r'^Status: Renamed mpcCalculator to mpcCalc2: 19 places in 2 files\.$',
		 r'^Status: Renamed mpcCalc2 to mpcCalc3: 19 places in 2 files\.$',
		 r'^Rename: mpcCalc3 \[CCalculatorParser::mpcCalc3\] at 19 places, 0 overrides$'], [r'Rename stopped']),
	('rename-class-twice', {
		#A class renamed (its header, its constructor and destructor, files opened as hidden tabs) and renamed again: the
		#second rename finds every place by the new name, as the renamed files are indexed again before it looks.
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '33:8',
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
		 r'^  Sub/Angle\.cpp 1:11 8$', r'^  Sub/Indented\.cpp 1:16 8$', r'^  Sub/User\.cpp 1:14 8$', r'^  Tool/Tool\.cpp 1:14 8$'],
		[r'OtherUser', r'Commented\.cpp [13]:']),
	('rename-file-related', dict(MakeRenameFileCopy(), **{
		#Rename File (step 3) on Engine.h with its related files, Engine.cpp and Engine_Linux.cpp (not EngineCache.h): the
		#files and both projects' items renamed, the 7 #includes changed and saved, Engine.h's tab following it.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h',
		'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:printincludes,printprojectfiles,printtabs,printdiskfile:Sub/User.cpp,'
		         'printdiskfile:Tool/Tool.vcxproj,printdiskfile:CPPEngine_Linux.cpp,printdiskfile:Commented.cpp'}),
		[r'^Status: Renamed Engine\.h to CPPEngine\.h and 2 more files: 7 #includes in 7 files, 2 projects\.$',
		 r'^Includes of CPPEngine\.h: 7$', r'^  Sub/Indented\.cpp 1:16 11$',
		 r'^Project files: Commented\.cpp CPPEngine\.cpp CPPEngine_Linux\.cpp Other/OtherUser\.cpp Sub/Angle\.cpp Sub/Indented\.cpp Sub/User\.cpp '
		 r'CPPEngine\.h EngineCache\.h Other/Engine\.h Tool/Tool\.cpp$',
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
		#Engine.h's tab, the #includes as they were, saved.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h',
		'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:undoall,printprojectfiles,printtabs,printdiskfile:Sub/User.cpp,printdiskfile:Engine.cpp,'
		         'printdiskfile:Tool/Tool.vcxproj'}),
		[r'^Status: Renamed Engine\.h to CPPEngine\.h and 2 more files: 7 #includes in 7 files, 2 projects\.$',
		 r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp Other/OtherUser\.cpp Sub/Angle\.cpp Sub/Indented\.cpp Sub/User\.cpp '
		 r'Engine\.h EngineCache\.h Other/Engine\.h Tool/Tool\.cpp$',
		 r'^Tabs: Engine\.h ', r'^#include "\.\./Engine\.h"', r'^#include "Engine\.h"', r'<ClInclude Include="\.\.\\Engine\.h" />'],
		[r"Couldn't rename", r'CPPEngine\.h"', r'Disk file .*:\n\(none\)']),
	('rename-file-redo', dict(MakeRenameFileCopy(), **{
		#Redo Across Files after the undo renames them all again.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h',
		'DEBUG': 'edit:renamefilerelated:CPPEngine.h,edit:undoall,edit:redoall,printprojectfiles,printtabs,printdiskfile:Sub/User.cpp,printdiskfile:CPPEngine.cpp'}),
		[r'^Project files: Commented\.cpp CPPEngine\.cpp CPPEngine_Linux\.cpp .* CPPEngine\.h EngineCache\.h',
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
		[r'^Menu Editor/Refactor: Rename File\.\.\., Extract Method', r'^Menu Explorer: .*, Rename\.\.\.(\(disabled\))?$',
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
		 r'^Project files: .* Sub/Customer\.cpp '],
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
		 r'^Project files: Commented\.cpp Motor\.cpp Motor_Linux\.cpp Other/OtherUser\.cpp .* Motor\.h EngineCache\.h Other/Engine\.h Tool/Tool\.cpp$',
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
	('rename-file-refused', dict(MakeRenameFileCopy(), **{
		#A name that is taken, one with a directory, and the same name: nothing is renamed.
		'WAIT_INDEX': '1', 'OPEN': 'Engine.h', 'DEBUG': 'edit:renamefile:EngineCache.h,edit:renamefile:Sub/Engine.h,edit:renamefile:Engine.h,printprojectfiles'}),
		[r"^Status: Can't rename: EngineCache\.h is already there\.$",
		 r"^Status: Can't rename: a file's new name can't be empty or have directories in it\.$",
		 r"^Status: Can't rename: Engine\.h is already called that\.$",
		 r'^Project files: Commented\.cpp Engine\.cpp Engine_Linux\.cpp .* Engine\.h EngineCache\.h'],
		[r'^Status: Renamed']),
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
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:2', 'TYPE': 'int miTypedMember; ',
		'DEBUG': 'printspans,wait:1500,printedits,printspans,edit:caret:38:7,edit:renamable'},
		[r'^Spans: same$', r'^Edits thread: idle$', r'^Spans: changed$',
		 r'^Rename: miTypedMember \[CCalcParentheses::miTypedMember\] at 1 places, 0 overrides$'], []),
	('edits-kept-after-configuration', {
		#The index built again for another configuration has the open files' edits, not just their files.
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'int iTypedName; ',
		'DEBUG': 'wait:1500,configuration:Debug|W65C816,waitindex,edit:caret:36:7,edit:renamable'},
		[r'^Rename: iTypedName \[.*CalculatorParser\.cpp:36:6:iTypedName\] at 1 places'], []),
	('nothing-at-caret', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '1:1', 'EDIT': 'usages,renamable'},
		[r'^Status: No usages found at the caret\.$', r'^Rename: nothing at the caret$'], []),
	('undo-across-files', {
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2',
		'EDIT': 'type:X,open:CalculatorSymbols.cpp,type:Y,undoall,print,undoall,print,redoall,redoall,print'},
		[r'^Edit menu: .*Undo Across Files Redo Across Files Find in Files\.\.\.$',
		 r'CalculatorParser\.cpp \* line 36: \[\tXmpcCalculator',
		 r'CalculatorSymbols\.cpp \(focus\) line 1: \[#include "Logger\.h"\]',
		 r'CalculatorParser\.cpp \(focus\) line 36: \[\tmpcCalculator',
		 r'CalculatorSymbols\.cpp \* \(focus\) line 1: \[Y#include'], []),
	('edit-menu-focus', {
		'OPEN': 'CalculatorParser.cpp', 'EDIT': 'focus:build,print,focus:claude,print'},
		[r'^Edit menu: Undo\(disabled\) Redo\(disabled\) Cut\(disabled\) Copy Paste\(disabled\) Paste from History\.\.\.\(disabled\) Select All',
		 r'^Edit menu: Undo\(disabled\) Redo\(disabled\) Cut Copy Paste Paste from History\.\.\. Select All'], []),
	('go-to-file', {
		'WAIT_INDEX': '1', 'GOTO': '1:bl/csym'},
		[r'^Go to \[bl/csym\]: 2 results', r'^  CalculatorSymbols\.h ', r'^  CalculatorSymbols\.cpp ',
		 r'^Opened .*[/\\]CalculatorSymbols\.h$'], []),
	('go-to-keys-change-tab', {
		#With the dialog open its own keys change its tab, searching for what is typed in it.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Cancel', 'DIALOG_KEYS': 'Control+Shift+N,Control+Alt+Shift+N,Control+N',
		'DEBUG': 'goto:0:calcpars,answered:1'},
		[r'^Keys Control\+Shift\+N: taken$', r'^Go to tab 1 \[calcpars\]: 2 results$', r'^  CalculatorParser\.h ',
		 r'^Keys Control\+Alt\+Shift\+N: taken$', r'^Go to tab 2 \[calcpars\]: 1 results$', r'^  CCalculatorParser ',
		 r'^Keys Control\+N: taken$', r'^Go to tab 0 \[calcpars\]: 1 results$', r'^Focus: CTextField$'], [r'not taken']),
	('usages-row-parts', {
		#A usage's row has its line number dim and the usage's own text in the match font; a group's count is dim, a file
		#has its icon, and what is noted of a usage is dim.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,references,usageparts'},
		[r'^  CalcParentheses\.h\{d:  1\} \(icon\)$', r'^    \{d:38: \}bool  \{m:IsParentheses\}\(void\);\{d:  \[declaration\]\}$',
		 r'^  Calculator\.cpp\{d:  3\} \(icon\)$', r'^    \{d:391: \}if \(pcExpression->\{m:IsParentheses\}\(\)\)$'], [r'\{\?:', r'ERROR']),
	('usages-preview', {
		#Beside the tree the selected usage's file is shown at its line, coloured as an editor is; stepping to another
		#usage shows that; with the preview off nothing is shown.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
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
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,usagetoolbar,usagetool:Next Occurrence,printselected,usagetoolbar,usagetool:Group by Project,usagetoolbar,printusages,'
		        'usagetool:Preview,usagetool:Navigate with Single Click,usagetoolbar,usagetool:Collapse All,printrows,usagetool:Expand All,printrows,usagetool:Rerun,printusages'},
		[r'^Toolbar: Rerun, Previous Occurrence\(disabled\), Next Occurrence, Expand All, Collapse All, Group by Usage Type, Group by Project, Group by Class, Show Read Access\(on\), Show Write Access\(on\), Preview\(on\), Navigate with Single Click, Pin Tab, Scope$',
		 r'^Selected: 391: ', r'^Toolbar: Rerun, Previous Occurrence, Next Occurrence, ', r'Group by Project\(on\), Group by Class, Show Read Access\(on\), Show Write Access\(on\), Preview\(on\)',
		 r'^  BaseLib  3 usages$', r'Preview, Navigate with Single Click\(on\), Pin Tab, Scope$', r'^Rows: 1$', r'^Rows: 6$',
		 r'^Usages of IsParentheses and 1 override  3 usages$', r'^  BaseLib  3 usages$', r'^      391: '], [r'No tool button', r'ERROR']),
	('usages-tabs', {
		#Find Usages shows what it finds in the tab chosen unless it is pinned: then in a new tab, the pinned one kept.  A
		#tab that isn't pinned is used again, in its place.  A tab chosen shows its own usages; its cross closes it, pinned
		#or not.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,usages,usagetabs,usagetool:Pin Tab,usagetoolbar,usagetabs,focus:editor,caret:37:14,usages,usagetabs,usagetoolbar,'
		        'focus:editor,caret:36:14,usages,usagetabs,usagetab:* IsParentheses,usagetabs,printusages,usageclosetab:* IsParentheses,usagetabs,printusages'},
		[r'^Usage tabs: >IsParentheses$', r'Navigate with Single Click, Pin Tab\(on\), Scope$', r'^Usage tabs: >\* IsParentheses$',
		 r'^Usages of SetExpression', r'^Usage tabs: \* IsParentheses, >SetExpression$', r'Navigate with Single Click, Pin Tab, Scope$',
		 r'^Usages of Evaluate', r'^Usage tabs: \* IsParentheses, >Evaluate$',
		 r'^Usage tabs: >\* IsParentheses, Evaluate$',
		 r'^Usages of IsParentheses and 1 override  3 usages$', r'^Usage tabs: >Evaluate$', r'^Usages of Evaluate'], [r'ERROR', r'SetExpression, .*Evaluate|Evaluate, .*SetExpression']),
	('usages-scope-current-file', {
		#Find Usages' scope: with Current File the usages in other files aren't in the tree, whose first row says the scope.
		#The symbol is declared in CalcParentheses.h and only used in Calculator.cpp.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usagescope:4,usages,usagescope:4,references'},
		[r'^No usages of IsParentheses and 1 override in CalcParentheses\.h$',
		 r'^References to IsParentheses and 1 override in CalcParentheses\.h  1 reference$', r'^    38: bool  IsParentheses\(void\);'],
		[r'Calculator\.cpp', r'hidden', r'ERROR']),
	('usages-scope-project', {
		#The Project scope is the active file's project; Find All References uses the scope too.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usagescope:1,references'},
		[r'^References to IsParentheses and 1 override in BaseLib  7 references$', r'^  Calculator\.cpp  3$'], [r'ERROR']),
	('usages-scope-directory', {
		#A directory's scope: nothing of the symbol's is in a directory it isn't used in.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'FIND_DIRECTORY': os.path.join(ROOT, 'CommonLibraries', 'Libraries', 'WindowLib'),
		'EDIT': 'usagegroups:,usagescope:2,usages'},
		[r'^No usages of IsParentheses and 1 override in .*WindowLib$'], [r'Calculator\.cpp', r'ERROR']),
	('usages-settings-declarations', {
		#Find Usages (Alt+F7) has the declarations with the usages when its settings say so.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usagegroups:,usagescope:0:d,usagesonly,printusages'},
		[r'^References to IsParentheses and 1 override  7 references$'], [r'ERROR']),
	('usages-settings-dialog', {
		#Find Usages Settings: its dialog, answered Find, finds the usages of the symbol at the caret.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'DIALOG_ANSWER': 'OK',
		'EDIT': 'usagegroups:', 'DEBUG': 'command:Navigate.FindUsagesSettings,answered:1,edit:printusages'},
		[r'^Dialog \[Find Usages Settings\] answered OK$', r'^Usages of IsParentheses and 1 override  3 usages$'], [r'ERROR']),
	('usages-scope-menu', {
		#The toolbar's Scope: a menu of the scopes (the startup solution's too, with two solutions open), the tab's saying so; one chosen shows the usages already found that are
		#in it, as the IDE was when they were found, and choosing the solution's shows them all again.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'usagegroups:,references,usagetool:Scope,usagescopemenu,usagescopemenu:3,printusages,usagetool:Scope,usagescopemenu,usagescopemenu:0,printusages'},
		[r'^References to IsParentheses and 1 override  7 references$', r'^Scope menu \(open\): Solution  \(shown\), Startup Solution, Project, Current File, Open Files, Directory$',
		 r'^References to IsParentheses and 1 override in CalcParentheses\.h  1 reference$',
		 r'^Scope menu \(open\): Solution, Startup Solution, Project, Current File  \(shown\), Open Files, Directory$',
		 r'^References to IsParentheses and 1 override  7 references$'], [r'ERROR']),
	('usages-access-filters', {
		#Show Read Access and Show Write Access, both on to start with: off, a field's usages that read it, or write it, are
		#left out of the tree, whose first row says how many are hidden.
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.h', 'CARET': '32:17',
		'EDIT': 'usagegroups:t,usages,usagetoolbar,usagetool:Show Read Access,printusages,usagetoolbar,usagetool:Show Read Access,usagetool:Show Write Access,printusages'},
		[r'^Usages of mpcCalculator  18 usages$', r'^  Value read  16 usages$', r'^  Value write  2 usages$', r'Show Read Access\(on\), Show Write Access\(on\), Preview',
		 r'^Usages of mpcCalculator  18 usages  \(16 hidden\)$', r'^  Value write  2 usages$', r'^      36: mpcCalculator = pcCalculator;$',
		 r'Show Read Access, Show Write Access\(on\), Preview', r'^Usages of mpcCalculator  18 usages  \(2 hidden\)$', r'^  Value read  16 usages$'],
		[r'No tool button', r'ERROR']),
	('find-toolbar', {
		#The Find tool window's toolbar has no groupings; Rerun finds the text again.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfindtoolbar,findtool:Rerun,printfind'},
		[r'^Toolbar: Rerun, Previous Occurrence\(disabled\), Next Occurrence, Expand All, Collapse All, Preview\(on\), Navigate with Single Click$',
		 r"^Occurrences of 'calculatorparser' in the solution  13 matches in 2 files$"], [r'No tool button', r'ERROR']),
	('find-row-parts', {
		#The Find tool window's rows: the text found is in the match font whatever its case.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfindparts'},
		[r'^  CalculatorParser\.h\{d:  1 match\} \(icon\)$', r'^    \{d:29: \}class C\{m:CalculatorParser\}$',
		 r'^    \{d:35: \}memset\(this, 0, sizeof\(C\{m:CalculatorParser\}\)\);$'], [r'\{\?:', r'ERROR']),
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
		'WAIT_INDEX': '1', 'FIND': '0:c:calculatorparser'},
		[r'^Find \[calculatorparser\]: Nothing found', r'^Find window: 0 results'], [r'^Opened ', r'ERROR']),
	('find-in-project-words', {
		#In the active file's project, whole words with their case: not CCalculatorParser's.
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'FIND': '1:cw:Parse'},
		[r'^Find \[Parse\]: \d+\+? matches in \d+\+? files', r'^  CCalcObject\* CCalculatorParser::Parse\(const char\* szText\)  \[CalculatorParser\.cpp 57\]  match 32\+5$',
		 r"^Occurrences of 'Parse' in BaseLib  \d+ matches in \d+ files"], [r'\[CalculatorParser\.cpp 33\]', r'ERROR']),
	('find-in-directory', {
		#In a directory and under it, whatever is in a project.
		'WAIT_INDEX': '1', 'FIND': '2:-:class CCalculatorParser', 'FIND_DIRECTORY': os.path.join(ROOT, 'CommonLibraries', 'Libraries')},
		[r'^Find \[class CCalculatorParser\]: 1 match in 1 file', r'^  class CCalculatorParser  \[CalculatorParser\.h 29\]  match 0\+23$',
		 r"^Occurrences of 'class CCalculatorParser' in .*Libraries  1 match in 1 file$", r'^  CalculatorParser\.h  1 match$'], [r'ERROR']),
	('find-in-current-file', {
		#The Current File scope: only the active file is searched, though others have the text.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'FIND': '4:-:IsParentheses'},
		[r'^Find \[IsParentheses\]: 1 match in 1 file', r'^  bool  IsParentheses\(void\);  \[CalcParentheses\.h 38\]  match 6\+13$',
		 r"^Occurrences of 'IsParentheses' in CalcParentheses\.h  1 match in 1 file$"], [r'Calculator\.cpp', r'ERROR']),
	('find-in-open-files', {
		#The Open Files scope: the files with an editor's tab.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'FIND': '5:-:IsParentheses'},
		[r'^Find \[IsParentheses\]: \d+ match(es)? in \d+ files?', r'\[CalcParentheses\.h 38\]', r"^Occurrences of 'IsParentheses' in the open files  "],
		[r'\[Calculator\.cpp ', r'ERROR']),
	('find-dialog-open-in-find-window', {
		#The dialog shown, and Open in Find Window clicked.
		'WAIT_INDEX': '1', 'DIALOG_ANSWER': 'Open in Find Window', 'DEBUG': 'find:0:calculatorparser,answered:1,printfind'},
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
		[r'^Keys: Navigate\.GoToDefinition F12 bound$', r'^Keys: Navigate\.GoToClass Control\+N bound$',
		 r'^Keys: Build\.Build Control\+Shift\+B bound$', r'^Keys: Run\.Start Control\+F5 bound$', r'^Keys: Debug\.Start F5 bound$',
		 r'^Keys: Debug\.BreakAll Control\+Pause/Break bound$',
		 r'^Buttons: Start \[Run\.Start\]$'], [r'NOT BOUND', r'\[none\]']),
	('configuration', {
		'CONFIGURATION': 'Debug|W65C816', 'DEBUG': 'printconfig'},
		[r'^Configuration: \[Debug\|W65C816\] \[Debug\|W65C816\]: Debug\|W65C816, Release\|W65C816, Edit Configurations\.\.\.,$'], []),
	('system-directories-newest', {
		#The newest MinGW GCC by its number, not its name, and not a folder that isn't a version.
		'SYSTEM_ROOT': MakeSystemRoot(['9-win32', '13-win32', 'notes']), 'DEBUG': 'printsystem'},
		[r'^System directories: \S*/13-win32/include/c\+\+, \S*/13-win32/include/c\+\+/x86_64-w64-mingw32, \S*/13-win32/include, \S*/usr/share/mingw-w64/include,$'], [r'/9-win32', r'/notes']),
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
		#configurations: an image, and no card.
		'DIALOG_ANSWER': 'OK',
		'BUILD_EDITS': 'name:Release|W65C816;check;name:;check;name:Fine;solution:Debug|Win32;card:Card.img;check;cardpath:\\\\boot.exe;toolchain:GCC;check;toolchain:W65C816;check',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printconfig'}),
		[r'^Build dialog: refused \[Another build configuration has this name\.\]$',
		 r'^Build dialog: refused \[A build configuration needs a name\.\]$',
		 r'^Build dialog: refused \[A card and a path on it go together\.\]$',
		 r'^Build dialog: refused \[Only an HCP65 toolchain can link a program, load files or use a card\.\]$',
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
		'DIALOG_ANSWER': 'Cancel',
		'RUN_EDITS': 'add;name:Debug|W65C816;check;name:;check;name:Fine;check;remove;remove;remove;check',
		'DEBUG': 'command:Run.EditConfigurations,answered:1,printconfig'},
		[r'^Run dialog: refused \[Another run configuration has this name\.\]$',
		 r'^Run dialog: refused \[A run configuration needs a name\.\]$',
		 r'^Run dialog: OK \[\]$', r'^Run dialog: OK \[\]$',
		 r'^Configuration: \[Debug\|W65C816\] \[Debug\|W65C816\]: Debug\|W65C816, Release\|W65C816, Edit Configurations\.\.\.,$'], []),
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
	('menus', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DEBUG': 'printmenus,clickmenu:View/Nope'},
		[r'^Menu File: Open Project/Solution\.\.\., Add Solution\.\.\., Close Solution, Recent Solutions>, Clear Recent Solutions, Save \[Ctrl\+S\], Save All \[Ctrl\+Shift\+S\], Compare with File\.\.\., Settings\.\.\. \[Ctrl\+Alt\+S\], Exit$',
		 r'^Menu File/Recent Solutions: 1 ',
		 r'^Menu Edit: Undo \[Ctrl\+Z\]\(disabled\), Redo \[Ctrl\+Shift\+Z\]\(disabled\), Cut \[Ctrl\+X\], Copy \[Ctrl\+C\], Paste \[Ctrl\+V\], Paste from History\.\.\. \[Ctrl\+Shift\+V\], Select All \[Ctrl\+A\], Extend Selection \[Ctrl\+W\], Shrink Selection \[Ctrl\+Shift\+W\], Undo Across Files\(disabled\), Redo Across Files\(disabled\), Find in Files\.\.\. \[Ctrl\+Shift\+F\]$',
		 r'^Menu Navigate: Back \[Ctrl\+Alt\+Left\](\(disabled\))?, Forward \[Ctrl\+Alt\+Right\](\(disabled\))?, Class\.\.\. \[Ctrl\+N\], File\.\.\. \[Ctrl\+Shift\+N\], Symbol\.\.\. \[Ctrl\+Alt\+Shift\+N\]$',
		 r'^Menu View: Solution Explorer, Find Usages, Find, Build, Indexing, Run, Debug, Claude$',
		 r'^Menu Git: Compare with HEAD, Compare with Branch\.\.\., Compare with Revision\.\.\.$',
		 r'^Menu Build: Build \[Ctrl\+Shift\+B\], Edit Build Configurations\.\.\.$',
		 r'^Menu Debug: Start Debugging \[F5\], Start Without Debugging \[Ctrl\+F5\], Stop Debugging \[Shift\+F5\], Stop Running\(disabled\), Restart \[Ctrl\+Shift\+F5\], Break All \[Ctrl\+Pause/Break\], Attach to Emulator\.\.\., Step Into \[F11\], Step Over \[F10\], Step Out \[Shift\+F11\], Toggle Breakpoint \[F9\], ',
		 r'^Menu Window: Reset Window Layout, Close All Documents$',
		 r'^Menu Editor: Go to Source \[Ctrl\+B\], Find Usages \[Alt\+F7\], Find Usages Settings\.\.\. \[Ctrl\+Alt\+Shift\+F7\], Rename\.\.\. \[Shift\+F6\], Refactor>, Cut \[Ctrl\+X\], Copy \[Ctrl\+C\], Paste \[Ctrl\+V\], Paste from History\.\.\. \[Ctrl\+Shift\+V\]$',
		 r'^Menu Editor/Refactor: Rename File\.\.\., Extract Method\.\.\.\(disabled\)$',
		 r'^Menu Explorer: Set as Startup Solution, Set as Startup Project\(disabled\), Remove Solution, Rename\.\.\.(\(disabled\))?$',
		 r'^Menu item \[View/Nope\] not found$'], []),
	('menu-clicks', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'clickmenu:Editor/Rename...,answered:1,clickmenu:Navigate/File...,answered:2,clickmenu:Editor/Find Usages,'
			'clickmenu:Editor/Go to Source,printtabs,clickmenu:Edit/Select All,clickmenu:View/Build,'
			'clickmenu:Window/Reset Window Layout,clickmenu:Explorer/Set as Startup Solution,clickmenu:File/Save All,printtabs'},
		[r'^Dialog \[Rename\] answered Cancel$', r'^Dialog \[Go To\] answered Cancel$',
		 r'^Tabs: .*>CalculatorParser\.h$', r'^Tabs: .*>CalculatorParser\.h$'], [r'not found', r'disabled']),
	('hierarchy-after-edits', {
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14',
		'EDIT': 'renamable,replace:29| : public CCalcExpression|,renamable'},
		[r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\)\] at 7 places, 1 overrides$',
		 r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\)\] at 2 places, 0 overrides$'], []),
	('virtual-after-edits', {
		'WAIT_INDEX': '1', 'OPEN': 'CalcExpression.h', 'CARET': '37:18',
		'EDIT': 'renamable,replace:37|virtual |,caret:37:9,renamable'},
		[r'^Rename: IsParentheses \[CCalcExpression::IsParentheses\(\)\] at 7 places, 1 overrides$',
		 r'^Rename: IsParentheses \[CCalcExpression::IsParentheses\(\)\] at 5 places, 0 overrides$'], []),
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
		#What types can't be had by anything, and what a dialog or text field needs only by an editor's.
		'DIALOG_ANSWER': 'OK', 'KEYMAP': MakeKeymapFile(),
		'KEYMAP_EDITS': 'select:Debug.StepOver;add:Q;add:Shift+Q;add:Space;add:Enter;add:Control+C;add:Cursor Up;add:F10;add:Control+D;select:Editor.Up;add:Q;add:F8;select:Edit.Copy;add:Tab',
		'DEBUG': 'command:File.Settings,answered:1,printkeys'},
		[r'^Keymap: Q types a character: add Ctrl or Alt\.$', r'^Keymap: Shift\+Q types a character: add Ctrl or Alt\.$', r'^Keymap: Space types a character: add Ctrl or Alt\.$',
		 r'^Keymap: Enter is needed by dialogs and text fields\.$', r'^Keymap: Ctrl\+C moves here|^Keymap: Ctrl\+C is needed by dialogs and text fields\.$', r'^Keymap: Up is needed by dialogs and text fields\.$',
		 r'^Keymap: F10 is already its shortcut\.$', r'^Keymap: Ctrl\+D moves here from Editor Actions: Duplicate\.$',
		 r'^Keymap: Q types a character: add Ctrl or Alt\.$', r'^Keymap: F8 is free\.$', r'^Keymap: Tab moves here from Editor Actions: Tab\.$',
		 r'^Keys: Edit\.Copy Tab bound$', r'^Keys: Editor\.Up Cursor Up bound$', r'^Keys: Editor\.Up F8 bound$', r'^Keys: Debug\.StepOver F10 bound$', r'^Keys: Debug\.StepOver Control\+D bound$'],
		[r'Keys: Debug\.StepOver Q', r'Keys: Debug\.StepOver Enter', r'Keys: Editor\.Duplicate', r'Keys: Editor\.Tab', r'NOT BOUND', r'ERROR']),
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
		[r'^Selection: \[IsParentheses\] on line 38, caret line 38$', r'^Selection: \[\tbool\t\tIsParentheses\(void\);$', r'^\] on line 39, caret line 39$'], []),
	('editor-drag-text', {
		#Dragging the selected text moves it: IsParentheses, selected by a double click, is pressed on and dropped at
		#the start of line 39's "void", where it stays selected.  Real presses through the window.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1',
		'DEBUG': 'wait:1500,edit:doubleclick:38:10,wait:1000,edit:dragtext:38:12:39:2,edit:printselection,edit:print'},
		[r'^Selection: \[IsParentheses\] on line 39, caret line 39$', r'CalcParentheses\.h \* \(focus\) line 39: \[\tIsParenthesesvoid\t\tPrint\(CChars\* psz\);\]$'], []),
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
		'WAIT_INDEX': '1', 'OPEN': 'Calculator.cpp', 'CARET': '363:21',
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
		[r'^Box: \[void\] \[bool\] \[void\]$', r'^Box: \[\] \[\] \[\]$', r'CalcParentheses\.h \* \(focus\) line 39: \[\tX\t\tPrint\(CChars\* psz\);\]$',
		 r'^Box: none$', r'CalcParentheses\.h \(focus\) line 39: \[\tvoid\t\tPrint\(CChars\* psz\);\]$'], []),
	('editor-gutter-select', {
		#Pressing a line's number in the gutter selects the line with its line feed; a shift press on line 36's number
		#then selects from where that selection started (line 38's start) up to line 36's start.  Real presses through
		#the window.
		'OPEN': 'CalcParentheses.h', 'CARET': '38:1',
		'DEBUG': 'wait:1500,edit:gutterclick:38,edit:printselection,wait:1000,edit:shiftgutterclick:36,edit:printselection'},
		[r'^Selection: \[\tbool\t\tIsParentheses\(void\);$', r'^\] on line 39, caret line 39$', r'^Selection: \[\tCNumber\t\tEvaluate\(void\);$', r'^\tvoid\t\tSetExpression\(CCalcExpression\* pcExpression\);$', r'^\] on line 38, caret line 36$'], []),	('editor-paste-history', {
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
]


def Hashes():
	asHashes = {}
	for szFile in WATCHED:
		if os.path.exists(szFile):
			asHashes[szFile] = hashlib.md5(open(szFile, 'rb').read()).hexdigest()
	return asHashes


def Run(szExecutable, szName, asVariables):
	asEnv = dict(os.environ)
	szSnapshot = os.path.join(tempfile.gettempdir(), 'IDEChecks-%d.png' % os.getpid())
	asEnv['HCP65_IDE_SNAPSHOT'] = szSnapshot
	asEnv['HCP65_IDE_SNAPSHOT_TICKS'] = '3'
	if CHECKS_RECENT and ('RECENT' not in asVariables):
		asEnv['HCP65_IDE_RECENT'] = CHECKS_RECENT
	aszArguments = []
	for szKey, szValue in asVariables.items():
		if szKey == 'SOLUTION':
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
