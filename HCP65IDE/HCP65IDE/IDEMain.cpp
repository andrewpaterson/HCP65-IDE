/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "BaseLib/Socket.h"
#include "StandardLib/Objects.h"
#include "IDEApp.h"
#include "IDEMain.h"


//////////////////////////////////////////////////////////////////////////
//
// Before the window factory is made.
//
//////////////////////////////////////////////////////////////////////////
void StartIDE(void)
{
	gcLogger.Init();
	TypesInit();
	TypeConverterInit();
	MemoryInit();
	DataIOInit();
	ObjectsInit();
	SocketsInit();  //The debugger's connection to the emulator.
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE's window, showing paszSolutions (one or more solutions, or a
// workspace, opened together; none, the IDE starts with nothing open)
// until it is closed.  Run from the HCP65IDE directory, the resources
// and WinInputLib's XML are found relative to it.
//
//////////////////////////////////////////////////////////////////////////
bool RunIDE(CNativeWindowFactory* pcFactory, CNativeInput* pcInput, CArrayChars* paszSolutions)
{
	Ptr<CIDEApp>	pApp;
	bool			bResult;

	//Validating every object each time one changes is too slow for the text's glyphs.  They are validated once at the end.
	gcObjects.DisableValidation();

	//The app (its window, content and input) is held by the root while it runs.
	pApp = OMalloc<CIDEApp>();
	ORoot()->Add(pApp);
	pApp->Setup(pcFactory, pcInput, "../../../CommonLibraries/Libraries/WinInputLib/xml", paszSolutions);
	bResult = pApp->Run();
	ORoot()->Remove(pApp);
	pApp = NULL;

	gcObjects.EnableValidation();
	gcObjects.ValidateObjectsConsistency();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// After the window factory is killed.  Returns the program's exit code.
//
//////////////////////////////////////////////////////////////////////////
int StopIDE(bool bResult)
{
	ObjectsFlush();
	SocketsKill();
	ObjectsKill();
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();
	gcLogger.Kill();

	return bResult ? 0 : 1;
}

