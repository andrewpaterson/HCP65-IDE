#!/usr/bin/env python3
#The HCP65 IDE's checks: the IDE is run scripted (HCP65_IDE_* variables, see CIDEApp::Setup and CIDEScript) on the
#HCP65Kernal solution, as last opened, and what it prints is checked.  The files it is pointed at (in commonlibraries)
#must be as committed, and none of them, nor the user's layout, workspace or recent solutions, may change.
#
#  IDEChecks.py [ide executable] [check name...]
#
#The executable defaults to ../../../build/HCP65IDERelease/HCP65IDE.  Exits non-zero if a check fails.
import hashlib, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
IDE_DIR = os.path.join(HERE, '..', 'HCP65IDE')
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
BASELIB = os.path.join(ROOT, 'commonlibraries', 'Libraries', 'BaseLib')
KERNAL = os.path.join(ROOT, 'HCP65', 'Kernal')

#Files the checks edit in the IDE (never saving them) and the user's files a scripted run mustn't write.
WATCHED = [os.path.join(BASELIB, f) for f in ['CalculatorParser.cpp', 'CalculatorParser.h', 'CalculatorSymbols.cpp', 'CalcParentheses.h', 'CalcExpression.h']] + \
          [os.path.join(KERNAL, 'HCP65Kernal.layout.user'), os.path.join(KERNAL, 'HCP65Kernal.hcp65workspace'),
           os.path.expanduser('~/.config/HCP65IDE/RecentSolutions.xml')]
BREAKPOINTS = os.path.join(KERNAL, 'HCP65Kernal.hcp65user')

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
		#Typed, given to the edits thread (held back), typed again while it is busy: what it made is of the old text,
		#so it isn't shown; the new text is given to it once it has finished, and shown.
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:2', 'TYPE': 'int iTypedName; ', 'EDITS_DELAY': '5000',
		'DEBUG': 'printspans,wait:1500,printedits,edit:type:x,wait:5500,printedits,printspans,wait:5500,printedits,printspans,'
		         'edit:caret:36:7,edit:renamable'},
		[r'^Spans: changed$', r'^Edits thread: busy$', r'^Edits thread: busy$', r'^Spans: same$', r'^Edits thread: idle$',
		 r'^Spans: changed$', r'^Rename: iTypedName \[.*CalculatorParser\.cpp:36:6:iTypedName\] at 1 places'], []),
	('header-edit-in-unit', {
		#A header's edits are worked out in the unit that includes it: its new member is indexed as the class's.
		'WAIT_INDEX': '1', 'OPEN': 'CalcParentheses.h', 'CARET': '38:2', 'TYPE': 'int miTypedMember; ',
		'DEBUG': 'printspans,wait:1500,printedits,printspans,edit:caret:38:7,edit:renamable'},
		[r'^Spans: changed$', r'^Edits thread: idle$', r'^Spans: changed$',
		 r'^Rename: miTypedMember \[CCalcParentheses::miTypedMember\] at 1 places, 0 overrides$'], []),
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
		'DEBUG': 'closetab,wait:300,printtabs,edit:caret:26:12,gotosource,printtabs,switchfile,printtabs'},
		[r'^Tabs: .* >CalculatorParser\.cpp\*$', r'^Tabs: .*CalculatorParser\.cpp\* >CalculatorParser\.h$',
		 r'^Tabs: .* >CalculatorParser\.cpp\* CalculatorParser\.h$'], []),
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
		'DEBUG': 'clickmenu:Git/Compare with Revision...,wait:300,printdiffs,clickmenu:Git/Compare with Branch...,wait:300,printdiffs'},
		[r'^Diffs: \[>CalculatorParser\.cpp \(HEAD~1 vs\. Your version\)\]$',
		 r'^Diffs: \[CalculatorParser\.cpp \(HEAD~1 vs\. Your version\)\] \[>CalculatorParser\.cpp \(\w+ vs\. Your version\)\]$'], []),
	('debug-dialog', {
		'OPEN': 'Kernal.cpp', 'DIALOG_ANSWER': 'Cancel', 'DEBUG': 'clickmenu:Debug/New Breakpoint at Address...,wait:300,printtabs'},
		[r'^Tabs: >Kernal\.cpp'], [r'not found', r'disabled']),
	('configuration', {
		'CONFIGURATION': 'Debug|W65C816', 'DEBUG': 'printconfig'},
		[r'^Configuration: \[Debug\] \[W65C816\]$'], []),
	('explorer', {
		'OPEN': 'CalculatorParser.cpp', 'EDIT': 'explorer', 'DEBUG': 'printexplorer'},
		[r'^Explorer: \[.*/BaseLib/CalculatorParser\.cpp\]$'], []),
	('breakpoint', {
		'OPEN': 'Kernal.cpp', 'DEBUG': 'line:20,buildtab,wait:300,printtabs'},
		[r'^Tabs: >Kernal\.cpp'], []),
	('menus', {
		'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DEBUG': 'printmenus,clickmenu:View/Nope'},
		[r'^Menu File: Open Project/Solution\.\.\., Add Solution\.\.\., Close Solution, Recent Solutions>, Clear Recent Solutions, Save, Save All, Compare with File\.\.\., Exit$',
		 r'^Menu File/Recent Solutions: 1 ',
		 r'^Menu Edit: Undo\(disabled\), Redo\(disabled\), Cut, Copy, Paste, Select All, Undo Across Files\(disabled\), Redo Across Files\(disabled\)$',
		 r'^Menu Navigate: Class\.\.\., File\.\.\., Symbol\.\.\.$',
		 r'^Menu View: Solution Explorer, Find Usages, Build, Debug, Claude$',
		 r'^Menu Git: Compare with HEAD, Compare with Branch\.\.\., Compare with Revision\.\.\.$',
		 r'^Menu Debug: Start Debugging \(F5\), ',
		 r'^Menu Window: Reset Window Layout, Close All Documents$',
		 r'^Menu Editor: Go to Source, Find Usages, Rename\.\.\., Refactor>$',
		 r'^Menu Editor/Refactor: Extract Method\.\.\.\(disabled\)$',
		 r'^Menu Explorer: Set as Startup Solution, Remove Solution$',
		 r'^Menu item \[View/Nope\] not found$'], []),
	('menu-clicks', {
		'WAIT_INDEX': '1', 'OPEN': 'CalculatorParser.cpp', 'CARET': '36:4', 'DIALOG_ANSWER': 'Cancel',
		'DEBUG': 'clickmenu:Editor/Rename...,wait:300,clickmenu:Navigate/File...,wait:300,clickmenu:Editor/Find Usages,'
			'clickmenu:Editor/Go to Source,printtabs,clickmenu:Edit/Select All,clickmenu:View/Build,'
			'clickmenu:Window/Reset Window Layout,clickmenu:Explorer/Set as Startup Solution,clickmenu:File/Save All,printtabs'},
		[r'^Dialog \[Rename\] answered Cancel$', r'^Dialog \[Go To\] answered Cancel$',
		 r'^Tabs: .*>CalculatorParser\.h$', r'^Tabs: .*>CalculatorParser\.h$'], [r'not found', r'disabled']),
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
	for szKey, szValue in asVariables.items():
		if szValue is None:
			asEnv.pop('HCP65_IDE_' + szKey, None)
		else:
			asEnv['HCP65_IDE_' + szKey] = szValue
	cResult = subprocess.run([szExecutable], cwd=IDE_DIR, env=asEnv, capture_output=True, timeout=300)
	if os.path.exists(szSnapshot):
		os.remove(szSnapshot)
	szOut = cResult.stdout.decode('latin-1') + cResult.stderr.decode('latin-1')
	return cResult.returncode, [l for l in szOut.split('\n') if not l.startswith('Process (')]


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
