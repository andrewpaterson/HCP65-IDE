#!/usr/bin/env python3
#The HCP65 IDE's checks: the IDE is run scripted (HCP65_IDE_* variables, see CIDEApp::Setup and CIDEScript) on the
#HCP65Kernal solution, as last opened, and what it prints is checked.  The files it is pointed at (in commonlibraries)
#must be as committed, and none of them, nor the user's layout, workspace or recent solutions, may change.
#
#  IDEChecks.py [ide executable] [check name...]
#
#The executable defaults to ../../../build/HCP65IDERelease/HCP65IDE.  Exits non-zero if a check fails.
#With HCP65_IDE_VERIFY_REDRAW set, every partial paint is checked too, and one that differed fails its check.
import atexit, hashlib, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
IDE_DIR = os.path.join(HERE, '..', 'HCP65IDE')
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
BASELIB = os.path.join(ROOT, 'commonlibraries', 'Libraries', 'BaseLib')
KERNAL = os.path.join(ROOT, 'HCP65', 'Kernal')

#Files the checks edit in the IDE (never saving them) and the user's files a scripted run mustn't write.
WATCHED = [os.path.join(BASELIB, f) for f in ['CalculatorParser.cpp', 'CalculatorParser.h', 'CalculatorSymbols.cpp', 'CalcParentheses.h', 'CalcExpression.h']] + \
          [os.path.join(KERNAL, 'HCP65Kernal.layout.user'), os.path.join(KERNAL, 'HCP65Kernal.hcp65workspace'), os.path.join(KERNAL, 'HCP65Kernal.hcp65build'),
           os.path.expanduser('~/.config/HCP65IDE/RecentSolutions.xml')]
BREAKPOINTS = os.path.join(KERNAL, 'HCP65Kernal.hcp65user')



def MakeSystemRoot(aszVersions, bLibrary=True):
	#A folder laid out as a machine with these MinGW GCC folders (and their C library's headers) is, for HCP65_IDE_SYSTEM_ROOT.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	for szVersion in aszVersions:
		os.makedirs(os.path.join(szRoot, 'usr/lib/gcc/x86_64-w64-mingw32', szVersion, 'include/c++/x86_64-w64-mingw32'))
	if bLibrary:
		os.makedirs(os.path.join(szRoot, 'usr/share/mingw-w64/include'))
	return szRoot


def MakeSolutionCopy():
	#A copy of a test solution with a comment in its build configurations, to save them to: a scripted run told to keep
	#its settings with SAVE_LAYOUT, and its recent solutions apart, writes only beside the copy.
	szRoot = tempfile.mkdtemp(prefix='IDEChecks-')
	atexit.register(shutil.rmtree, szRoot, True)
	szKernel = os.path.join(szRoot, 'Workspace', 'Kernel')
	shutil.copytree(os.path.join(ROOT, 'HCP65-IDE', 'Tests', 'TestBuildLib', 'TestBuildLib', 'Input', 'Workspace'), os.path.join(szRoot, 'Workspace'))
	szFile = os.path.join(szKernel, 'Kernel.hcp65build')
	szText = open(szFile, newline='').read().replace('<BuildConfigurations>', '<!-- A comment to keep. -->\n<BuildConfigurations>', 1)
	open(szFile, 'w', newline='').write(szText)
	return {'SOLUTION': os.path.join(szKernel, 'Kernel.sln'), 'SAVE_LAYOUT': '1', 'RECENT': os.path.join(szRoot, 'Recent.xml')}


#Each check: its name, its HCP65_IDE_* variables, the lines (regular expressions) its output must have, in order, and
#lines it mustn't have.
CHECKS = [
	('usages-rename', {
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:14', 'EDIT': 'usages,references,renamable'},
		[r'^Usages of IsParentheses and 1 override  3 usages$',
		 r'^      Calculator\.cpp 367:',
		 r'^References to IsParentheses and 1 override  7 references$',
		 r'^      CalcParentheses\.h 38: .*\[declaration\]$',
		 r'^Rename: IsParentheses \[CCalcParentheses::IsParentheses\(\)\] at 7 places, 1 overrides$'], []),
	('rename-after-edits', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2',
		'EDIT': 'type:int i; ,rename:mpcCalc2,rename:mpcCalc3,renamable'},
		[r'^Status: Renamed mpcCalculator to mpcCalc2: 19 places in 2 files\.$',
		 r'^Status: Renamed mpcCalc2 to mpcCalc3: 19 places in 2 files\.$',
		 r'^Rename: mpcCalc3 \[CCalculatorParser::mpcCalc3\] at 19 places, 0 overrides$'], [r'Rename stopped']),
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
		[r'^Edit menu: .*Undo Across Files Redo Across Files$',
		 r'CalculatorParser\.cpp \* line 36: \[\tXmpcCalculator',
		 r'CalculatorSymbols\.cpp \(focus\) line 1: \[#include "Logger\.h"\]',
		 r'CalculatorParser\.cpp \(focus\) line 36: \[\tmpcCalculator',
		 r'CalculatorSymbols\.cpp \* \(focus\) line 1: \[Y#include'], []),
	('edit-menu-focus', {
		'OPEN': 'CalculatorParser.cpp', 'EDIT': 'focus:build,print,focus:claude,print'},
		[r'^Edit menu: Undo\(disabled\) Redo\(disabled\) Cut\(disabled\) Copy Paste\(disabled\) Select All',
		 r'^Edit menu: Undo\(disabled\) Redo\(disabled\) Cut Copy Paste Select All'], []),
	('go-to-file', {
		'WAIT_INDEX': '1', 'GOTO': '1:bl/csym'},
		[r'^Go to \[bl/csym\]: 2 results', r'^  CalculatorSymbols\.h ', r'^  CalculatorSymbols\.cpp ',
		 r'^Opened .*/CalculatorSymbols\.h$'], []),
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
		[r'^Tabs: >Kernal\.cpp'], [r'not found', r'disabled']),
	('commands', {
		'OPEN': 'Kernal.cpp', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'command:Debug.NewBreakpoint,answered:1,printtabs,command:Window.CloseAllDocuments,printtabs,command:No.Such'},
		[r'^Tabs: >Kernal\.cpp', r'^Tabs:\s*$', r'^Command \[No\.Such\] not found'], []),
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
	('build-configuration-rules', {
		#OK is refused for what the file couldn't have, and the real file isn't written by a scripted run.
		'DIALOG_ANSWER': 'OK',
		'BUILD_EDITS': 'name:Release|W65C816;check;name:;check;name:Fine;solution:Debug|Win32;card:Card.img;check;cardpath:\\\\boot.exe;toolchain:GCC;check;toolchain:W65C816;check',
		'DEBUG': 'command:Build.EditConfigurations,answered:1,printconfig'},
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
		[r'^Explorer: \[.*/BaseLib/CalculatorParser\.cpp\]$'], []),
	('breakpoint', {
		'OPEN': 'Kernal.cpp', 'DEBUG': 'line:20,buildtab,wait:300,printtabs'},
		[r'^Tabs: >Kernal\.cpp'], []),
	('menus', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DEBUG': 'printmenus,clickmenu:View/Nope'},
		[r'^Menu File: Open Project/Solution\.\.\., Add Solution\.\.\., Close Solution, Recent Solutions>, Clear Recent Solutions, Save \[Ctrl\+S\], Save All \[Ctrl\+Shift\+S\], Compare with File\.\.\., Exit$',
		 r'^Menu File/Recent Solutions: 1 ',
		 r'^Menu Edit: Undo \[Ctrl\+Z\]\(disabled\), Redo \[Ctrl\+Shift\+Z\]\(disabled\), Cut \[Ctrl\+X\], Copy \[Ctrl\+C\], Paste \[Ctrl\+V\], Select All \[Ctrl\+A\], Undo Across Files\(disabled\), Redo Across Files\(disabled\)$',
		 r'^Menu Navigate: Class\.\.\. \[Ctrl\+N\], File\.\.\. \[Ctrl\+Shift\+N\], Symbol\.\.\. \[Ctrl\+Alt\+Shift\+N\]$',
		 r'^Menu View: Solution Explorer, Find Usages, Build, Run, Debug, Claude$',
		 r'^Menu Git: Compare with HEAD, Compare with Branch\.\.\., Compare with Revision\.\.\.$',
		 r'^Menu Build: Build \[Ctrl\+Shift\+B\], Edit Build Configurations\.\.\.$',
		 r'^Menu Debug: Start Debugging \[F5\], Start Without Debugging \[Ctrl\+F5\], Stop Debugging \[Shift\+F5\], Stop Running\(disabled\), Restart \[Ctrl\+Shift\+F5\], Break All \[Ctrl\+Pause/Break\], Attach to Emulator\.\.\., Step Into \[F11\], Step Over \[F10\], Step Out \[Shift\+F11\], Toggle Breakpoint \[F9\], ',
		 r'^Menu Window: Reset Window Layout, Close All Documents$',
		 r'^Menu Editor: Go to Source \[Ctrl\+B\], Find Usages \[Alt\+F7\], Rename\.\.\. \[Shift\+F6\], Refactor>$',
		 r'^Menu Editor/Refactor: Extract Method\.\.\.\(disabled\)$',
		 r'^Menu Explorer: Set as Startup Solution, Remove Solution$',
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
	('view-menu', {
		'OPEN': 'CalculatorParser.cpp', 'DEBUG': 'clickmenu:View/Debug,clickmenu:Window/Close All Documents,printtabs'},
		[r'^Tabs:$'], [r'not found', r'disabled']),
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


def Check(szExecutable, szName, asVariables, aszExpected, aszForbidden):
	asBefore = Hashes()
	bBreakpoints = os.path.exists(BREAKPOINTS)
	iCode, aszLines = Run(szExecutable, szName, asVariables)
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


def main():
	aszArgs = sys.argv[1:]
	szExecutable = os.path.join(ROOT, 'build', 'HCP65IDERelease', 'HCP65IDE')
	if aszArgs and os.path.isfile(aszArgs[0]):
		szExecutable = os.path.abspath(aszArgs.pop(0))
	iFailed = 0
	for szName, asVariables, aszExpected, aszForbidden in CHECKS:
		if aszArgs and szName not in aszArgs:
			continue
		if not Check(szExecutable, szName, asVariables, aszExpected, aszForbidden):
			iFailed += 1
	print('%d failed' % iFailed)
	return 1 if iFailed else 0


if __name__ == '__main__':
	sys.exit(main())
