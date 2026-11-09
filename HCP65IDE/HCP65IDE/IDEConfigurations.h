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
#ifndef __IDE_CONFIGURATIONS_H__
#define __IDE_CONFIGURATIONS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/CommandSet.h"
#include "BaseLib/Chars.h"
#include "BuildLib/BuildConfiguration.h"
#include "BuildLib/RunConfiguration.h"
#include "IDERunConfigurationsDialog.h"


class CIDEContent;


//The toolbar's run configuration, as IntelliJ IDEA has them: the workspace's run configurations (CRunConfiguration),
//the one chosen and Edit Configurations.  The one chosen says which build configuration is built and debugged, each
//solution's of that name, and the code model parses as its solution configuration.
class CIDEConfigurations : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEConfigurations);
DESTRUCTABLE(CIDEConfigurations);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	CChars						mszRunConfiguration;	//The chosen run configuration's name; empty if none.
	CChars						mszConfiguration;	//The name of the build configuration it builds; empty if none has it.
	Ptr<CComboBox>				mpRunConfigurations;  //Each run configuration, then Edit Configurations.
	Ptr<CIDERunConfigurationsDialog>	mpDialog;  //While it's shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	Ptr<CComboBox>		CreateComboBox(void);
	Ptr<CComboBox>		GetRunConfigurationBox(void);
	bool			SetRunConfiguration(const char* szName);
	CRunConfiguration*	GetRunConfiguration(void);
	char*			GetRunConfigurationName(void);
	void			SetRunConfigurationName(const char* szName);
	bool			SetConfiguration(const char* szConfiguration);
	CBuildConfiguration*	GetConfiguration(void);
	char*			GetConfigurationName(void);
	void			SetConfigurationName(const char* szName);
	bool			HasConfiguration(void);
	void				ShowConfigurations(void);

protected:
	void			RunConfigurationChosen(void);
	void			ShowConfiguration(void);
	void			RequestEditDialog(void);
	void			ShowEditDialog(void);
};


#endif // __IDE_CONFIGURATIONS_H__

