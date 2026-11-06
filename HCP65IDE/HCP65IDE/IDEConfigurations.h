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
#include "BuildLib/BuildConfiguration.h"


class CIDEContent;


//The toolbar's build configuration and platform, as Visual Studio has them: the solutions' configurations, the one
//chosen, and the code model parsing as its solution configuration.
class CIDEConfigurations : public CObject
{
friend class CIDEContent;
friend class CIDEScript;
CONSTRUCTABLE(CIDEConfigurations);
DESTRUCTABLE(CIDEConfigurations);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CComboBox>	mpConfigurations;  //The build configuration's configuration; the code is parsed as its solution configuration.
	Ptr<CComboBox>	mpPlatforms;  //And its platform.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				CreateComboBoxes(void);
	Ptr<CComboBox>		GetConfigurationBox(void);
	Ptr<CComboBox>		GetPlatformBox(void);
	bool			SetConfiguration(const char* szConfiguration);
	void			ConfigurationChosen(void);
	void			SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms);
	void			ShowConfiguration(void);
	void			SelectComboBoxItem(Ptr<CComboBox> pComboBox, const char* szText);
	CBuildConfiguration*	GetConfiguration(void);
	char*			GetConfigurationName(void);
	void				ShowConfigurations(void);

protected:
	CBuildConfiguration*	GetChosenConfiguration(void);
};


#endif // __IDE_CONFIGURATIONS_H__

