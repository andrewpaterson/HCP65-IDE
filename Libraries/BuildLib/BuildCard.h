/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __BUILD_CARD_H__
#define __BUILD_CARD_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"


#define BUILD_CARD_EXTENSION	".hcp65card"


//A card: an SD card's image and the programs on it, each a file a build makes and its path on the card's FAT volume.
//It is a file of its own (BootCard.hcp65card), committed, which the run configurations that debug on the card name
//(CRunConfiguration::GetCard), so a card many programs share is said once (ConfigurationsPlan.txt question 27):
//
//	<Card Image="..\HCP65-Emulator\TestHCP65Lib\TestHCP65Lib\Output\BootCard.img">
//		<Program File="..\Kernal\Output\$(Platform)\$(Configuration)\OS.SYS" Path="\OS\OS.SYS" Lines="Kernel"/>
//		<Program File="..\Init\Output\$(Platform)\$(Configuration)\INIT.EXE" Path="\OS\INIT.EXE"/>
//	</Card>
//
//Paths are relative to the card file.  $(Configuration) and $(Platform) in a program's file are those of the build
//configuration being debugged (Release and HCP65C816 of "Release|HCP65C816").  Lines says the program runs where it is
//linked, as the kernal the boot loader loads does: debugging reads its line table as it is, its breakpoints in that
//breakpoint space, when the workspace hasn't the solution that builds it.
class CBuildCard
{
protected:
	CChars		mszFile;
	CChars		mszImage;
	CArrayChars	maszFiles;
	CArrayChars	maszPaths;
	CArrayChars	maszLines;

public:
	void	Init(void);
	void	Kill(void);

	bool	Read(const char* szFile);
	bool	Write(const char* szFile);

	void	SetImage(const char* szImage);
	void	AddProgram(const char* szFile, const char* szPath, const char* szLines);

	char*	GetFile(void);
	void	GetImage(CChars* pszDest);
	size	NumPrograms(void) const;
	void	GetProgramFile(size uiIndex, const char* szConfiguration, CChars* pszDest);
	char*	GetProgramPath(size uiIndex);
	char*	GetProgramFileText(size uiIndex);
	char*	GetImageText(void);
	char*	GetProgramLines(size uiIndex);
	size	FindProgram(const char* szFile, const char* szConfiguration);

protected:
	void	FullPath(const char* szPath, CChars* pszDest);
};


#endif // __BUILD_CARD_H__

