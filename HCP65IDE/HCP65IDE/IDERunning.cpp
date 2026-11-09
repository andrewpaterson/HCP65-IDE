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
#include <string.h>
#include "BaseLib/Numbers.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDERunning.h"


#define IDE_RUN_POLL	50		//Milliseconds between looks at the running tool.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mcProcess.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CProcess, mcProcess);
	M_Pointer(mpRunPane);
	M_Pointer(mpRunTool);
	M_Pointer(mpStartButton);
}


//////////////////////////////////////////////////////////////////////////
//
// A tool still running is stopped.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::Free(void)
{
	mcProcess.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunning::Save(CObjectWriter* pcFile) { return false; }
bool CIDERunning::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Run tab, put at psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::CreatePane(SDockPlace* psPlace)
{
	mpRunPane = OMalloc<CIDEBuildPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetCodeFamily(), mpcContent->GetTheme()->GetColour(TC_Text));
	mpRunTool = mpcContent->GetDock()->AddPane(IDE_RUN_ID, "Run", NULL, mpRunPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Start Without Debugging (Control+F5, as Visual Studio's) and Stop
// Running.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::AddCommands(Ptr<CCommandSet> pCommands)
{
	Ptr<CCommand>	pStop;

	pCommands->Add("Run.Start", "Start Without Debugging", "Control+F5", Ptr<CImageCel>(), this, &CIDERunning::StartOrStop);
	pStop = pCommands->Add("Run.Stop", "Stop Running", "", Ptr<CImageCel>(), this, &CIDERunning::Stop);
	pStop->SetEnabledCheck(this, &CIDERunning::IsRunning);
}


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's Start button: Stop while the tool runs.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDERunning::CreateStartButton(void)
{
	mpStartButton = OMalloc<CButton>(mpcContent->GetWindow(), mpcContent->GetTheme(), "Start");
	mpStartButton->SetAlignment(CSH_Left, CSV_Center);
	mpStartButton->SetCommand(mpcContent->GetCommands()->Find("Run.Start"));
	return mpStartButton;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunning::StartOrStop(void)
{
	return IsRunning() ? Stop() : Start();
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen run configuration's tool starts, its arguments split at
// spaces.  False, with why in the Run tab, if there's nothing to run or it
// can't be started.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunning::Start(void)
{
	CChars		szExecutable;
	CChars		szDirectory;
	CChars		szArguments;
	CChars		szMessage;
	CArrayChars	aszSplit;
	CArrayChars	aszArguments;
	size		i;
	bool		bResult;

	mpRunPane->Clear();
	mpcContent->GetDock()->Show(mpRunTool);
	szExecutable.Init();
	szDirectory.Init();
	szArguments.Init();
	szMessage.Init();
	bResult = mpcContent->GetDebugging()->ChooseTool(&szExecutable, &szDirectory, &szArguments, &szMessage);
	if (bResult)
	{
		aszSplit.Init();
		aszArguments.Init();
		aszSplit.Split(szArguments.Text(), ' ');
		for (i = 0; i < aszSplit.NumElements(); i++)
		{
			if (aszSplit.GetText(i)[0] != '\0')
			{
				aszArguments.Add(aszSplit.GetText(i));
			}
		}
		szMessage.Set("Running: ");
		szMessage.Append(szExecutable);
		if (!szArguments.Empty())
		{
			szMessage.Append(' ');
			szMessage.Append(szArguments);
		}
		ShowMessage(szMessage.Text());
		bResult = mcProcess.Start(szExecutable.Text(), &aszArguments, szDirectory.Empty() ? NULL : szDirectory.Text());
		szMessage.Set("Could not start ");
		szMessage.Append(szExecutable);
		szMessage.Append('.');
		aszArguments.Kill();
		aszSplit.Kill();
	}
	if (bResult)
	{
		mpRunPane->SetProgress(1.0f, mpcContent->GetTheme()->GetColour(TC_Accent));
		mpStartButton->SetCaption("Stop");
		mpcContent->GetWindow()->RequestTick(IDE_RUN_POLL);
	}
	else
	{
		ShowMessage(szMessage.Text());
		mpRunPane->SetProgress(1.0f, mpcContent->GetTheme()->GetColour(TC_Failure));
	}
	szMessage.Kill();
	szArguments.Kill();
	szDirectory.Kill();
	szExecutable.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The next poll sees it has gone and says so.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunning::Stop(void)
{
	if (!IsRunning())
	{
		return false;
	}
	mcProcess.Terminate();
	mpcContent->GetWindow()->RequestTick(0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What the tool has written since last time, and whether it has finished.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::PollRun(Ptr<CWindow> pWindow)
{
	CArrayChars	aszLines;
	bool		bRunning;

	if (!IsRunning())
	{
		return;
	}
	aszLines.Init();
	bRunning = mcProcess.Poll(&aszLines);
	if (bRunning)
	{
		mpRunPane->AppendLines(&aszLines);
		pWindow->RequestTick(IDE_RUN_POLL);
	}
	else
	{
		Finished(&aszLines);
	}
	aszLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tool has exited: paszLines, the last it wrote, and its exit code.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::Finished(CArrayChars* paszLines)
{
	CChars	szMessage;
	int		iExitCode;

	iExitCode = mcProcess.GetExitCode();
	szMessage.Init("Exited with code ");
	szMessage.Append(iExitCode);
	szMessage.Append('.');
	paszLines->Add(szMessage.Text());
	szMessage.Kill();
	mpRunPane->AppendLines(paszLines);
	mpRunPane->SetProgress(1.0f, mpcContent->GetTheme()->GetColour((iExitCode == 0) ? TC_Success : TC_Failure));
	mpStartButton->SetCaption("Start");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunning::ShowMessage(const char* szMessage)
{
	CArrayChars	aszLines;

	aszLines.Init();
	aszLines.Add(szMessage);
	mpRunPane->AppendLines(&aszLines);
	aszLines.Kill();
}


Ptr<CButton> CIDERunning::GetStartButton(void) { return mpStartButton; }
Ptr<CIDEBuildPane> CIDERunning::GetRunPane(void) { return mpRunPane; }
bool CIDERunning::IsRunning(void) { return mcProcess.IsRunning(); }

