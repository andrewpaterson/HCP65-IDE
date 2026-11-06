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
#include "BaseLib/NativeThread.h"
#include "IDECodeModel.h"


//////////////////////////////////////////////////////////////////////////
//
// szText NULL: the file is read.
//
//////////////////////////////////////////////////////////////////////////
void SIDEEditedFile::Init(size uiProject, const char* szFileName, const char* szText, size uiChanges, bool bHighlight, bool bIndex)
{
	this->uiProject = uiProject;
	this->szFileName.Init(szFileName);
	bText = szText != NULL;
	this->szText.Init(bText ? szText : "");
	this->uiChanges = uiChanges;
	this->bHighlight = bHighlight;
	this->bIndex = bIndex;

	bUnit = false;
	uiUnitProject = 0;
	szUnit.Init();
	szUnitText.Init();
	bUnitText = false;

	acHighlights.Init();
	bHighlighted = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The file (a header) is parsed in the unit szUnit, from szText (NULL:
// the unit is read).
//
//////////////////////////////////////////////////////////////////////////
void SIDEEditedFile::SetUnit(size uiProject, const char* szUnit, const char* szText)
{
	bUnit = true;
	uiUnitProject = uiProject;
	this->szUnit.Set(szUnit);
	bUnitText = szText != NULL;
	szUnitText.Set(bUnitText ? szText : "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEditedFile::Kill(void)
{
	acHighlights.Kill();
	szUnitText.Kill();
	szUnit.Kill();
	szText.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The files are freed and the array emptied; it can be used again.
//
//////////////////////////////////////////////////////////////////////////
void KillIDEEditedFiles(CArrayIDEEditedFiles* papsFiles)
{
	size			i;
	SIDEEditedFile*	psFile;

	for (i = 0; i < papsFiles->NumElements(); i++)
	{
		psFile = papsFiles->GetPtr(i);
		psFile->Kill();
		free(psFile);
	}
	papsFiles->ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void IDEEditsJob(void* pvData)
{
	((CIDECodeModel*)pvData)->RunEditsThread();
}


//////////////////////////////////////////////////////////////////////////
//
// The edited files in papsFiles (which is emptied; the model has them
// now) worked out, in order: with bNow on this thread, otherwise on the
// edits thread, so the UI isn't held up while the user types.  Take them
// back with TakeEditedFiles, for their highlights; files given before and
// not taken are forgotten.  The index can be asked meanwhile: it has a
// file's old places until the file is merged.  Call it once the indexing
// has finished (IsIndexing) if any file is to be indexed, with each
// changed header's text already set (SetHeaderText).
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::ProcessEditedFiles(CArrayIDEEditedFiles* papsFiles, bool bNow)
{
	size	i;

	WaitForEditedFiles();
	KillIDEEditedFiles(&mapsEdited);
	for (i = 0; i < papsFiles->NumElements(); i++)
	{
		mapsEdited.Add(papsFiles->GetPtr(i));
	}
	papsFiles->ReInit();
	if (mapsEdited.NumElements() == 0)
	{
		return;
	}

	if (bNow)
	{
		for (i = 0; i < mapsEdited.NumElements(); i++)
		{
			ProcessEditedFile(mapsEdited.GetPtr(i));
		}
		return;
	}

	if (!mbEdits)
	{
		mcEdits.Init(1);
		mbEdits = true;
	}
	mcIndexMutex.Lock();
	mbEditsBusy = true;
	mcIndexMutex.Unlock();
	mcEdits.Run(&IDEEditsJob, this);
}


//////////////////////////////////////////////////////////////////////////
//
// On the edits thread: each file given to ProcessEditedFiles.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::RunEditsThread(void)
{
	size	i;

	if (miEditsDelay > 0)
	{
		ThreadSleep(miEditsDelay);
	}
	for (i = 0; i < mapsEdited.NumElements(); i++)
	{
		ProcessEditedFile(mapsEdited.GetPtr(i));
	}
	mcIndexMutex.Lock();
	mbEditsBusy = false;
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// The files given to ProcessEditedFiles, worked out, added to papsDest
// (the caller frees them, KillIDEEditedFiles).  Without bWait, false if
// the edits thread hasn't finished them yet.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::TakeEditedFiles(CArrayIDEEditedFiles* papsDest, bool bWait)
{
	size	i;

	if (!bWait && IsProcessingEditedFiles())
	{
		return false;
	}
	WaitForEditedFiles();
	for (i = 0; i < mapsEdited.NumElements(); i++)
	{
		papsDest->Add(mapsEdited.GetPtr(i));
	}
	mapsEdited.ReInit();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the edits thread is working out the files given it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::IsProcessingEditedFiles(void)
{
	bool	bBusy;

	mcIndexMutex.Lock();
	bBusy = mbEditsBusy;
	mcIndexMutex.Unlock();
	return bBusy;
}


//////////////////////////////////////////////////////////////////////////
//
// Returns once the edits thread has finished; what it made is kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::WaitForEditedFiles(void)
{
	if (mbEdits)
	{
		mcEdits.Wait();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One edited file parsed once, then highlighted and indexed again from
// that parse.  A header is parsed in its unit, with what comes before it
// there, and the unit's and the header's old places are forgotten; one
// its unit no longer includes is highlighted on its own.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::ProcessEditedFile(SIDEEditedFile* psFile)
{
	CIDEParsedUnit	cUnit;
	CCPPIndexUnit	cGathered;
	CChars			szErrors;
	CSourceFile*	pcFile;
	const char*		szText;
	bool			bParsed;

	szText = psFile->bText ? psFile->szText.Text() : NULL;
	szErrors.Init();
	cGathered.Init();
	cUnit.Init();
	if (psFile->bUnit)
	{
		bParsed = ParseUnit(psFile->uiUnitProject, psFile->szUnit.Text(), psFile->bUnitText ? psFile->szUnitText.Text() : NULL, &cUnit, &szErrors);
	}
	else
	{
		bParsed = ParseUnit(psFile->uiProject, psFile->szFileName.Text(), szText, &cUnit, &szErrors);
	}

	if (bParsed && psFile->bHighlight)
	{
		pcFile = psFile->bUnit ? FindFileInUnit(&cUnit, psFile->szFileName.Text()) : cUnit.mpcUnit;
		if (pcFile)
		{
			HighlightFile(&cUnit, pcFile, &psFile->acHighlights);
			psFile->bHighlighted = true;
		}
	}
	if (bParsed && psFile->bIndex)
	{
		GatherUnit(&cUnit, &cGathered, NULL);
		mcIndexMutex.Lock();
		mcIndex.RemoveFile(psFile->bUnit ? psFile->szUnit.Text() : psFile->szFileName.Text());
		if (psFile->bUnit)
		{
			mcIndex.RemoveFile(psFile->szFileName.Text());
		}
		mcIndex.Merge(&cGathered);
		mcIndexMutex.Unlock();
	}
	cUnit.Kill();

	if (psFile->bHighlight && !psFile->bHighlighted && psFile->bUnit)
	{
		cUnit.Init();
		if (ParseUnit(psFile->uiProject, psFile->szFileName.Text(), szText, &cUnit, &szErrors))
		{
			HighlightFile(&cUnit, cUnit.mpcUnit, &psFile->acHighlights);
			psFile->bHighlighted = true;
		}
		cUnit.Kill();
	}
	cGathered.Kill();
	szErrors.Kill();
}
