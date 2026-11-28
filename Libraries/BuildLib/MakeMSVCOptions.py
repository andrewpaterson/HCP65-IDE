#!/usr/bin/env python3
"""Writes BuildOptionsMSVC.inc, the rows of Edit Build Configurations' MSVC page (BuildOptionEdits.cpp; HCP65-IDE's
ConfigurationsPlan.txt, C9 stage 3), from Visual Studio's own descriptions of its property pages:

    python3 MakeMSVCOptions.py <MSBuild>/Microsoft/VC/v160/1033 > BuildOptionsMSVC.inc

The files there (RULES) say every setting of the project's own pages and of each tool's: its tag, the name its page
shows, the page's category it is in, for one with fixed values what they are, and where it is kept: in its tool's
group (ClCompile, Link, PreBuildEvent), as a property of the project (IncludePath), or as one of a configuration's own
(ConfigurationType).  Each is a row: one that is true or false or has values is chosen from them, a list adds up
(BuildOptionEdits.h, bAdds), and anything else is typed.

Left out, and listed at the top of the file: a setting the pages don't show (Visible="false"), one of the project's
globals (its SDK version: not a configuration's), and what is kept on each reference to another project.  The file
is written once and committed: run this again for another version of Visual Studio.
"""
import os
import sys
import textwrap
import xml.etree.ElementTree as ET

#Visual Studio's descriptions, in the order their pages are in the dialog: the file, the name its sections have,
#and whether its settings are the whole program's (the linker's) or each project's own.
RULES = (("general.xml", "Project", False), ("general_advanced.xml", "Project: Advanced", False), ("directories.xml", "Directories", False),
         ("cl.xml", "C/C++", False), ("link.xml", "Linker", True), ("lib.xml", "Librarian", False), ("rc.xml", "Resources", False),
         ("midl.xml", "MIDL", False), ("mt.xml", "Manifest Tool", False), ("bscmake.xml", "Browse Information", False),
         ("xdcmake.xml", "XML Document Generator", False), ("build_events.xml", "Build Events", False),
         ("custom_build_step.xml", "Custom Build Step", False))
KINDS = ("StringListProperty", "BoolProperty", "EnumProperty", "StringProperty", "IntProperty", "DynamicEnumProperty")
NOT_SETTINGS = ("ProjectReference",)		# Kept on each reference to another project, not for the project.


def quoted(text):
	return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def kept(source):
	"""Where a data source keeps a setting: the tool whose group it is in ("ClCompile"), "" for a property of the
	project, "Configuration" for one of a configuration's own; None for one the dialog doesn't write (the project's
	globals, which aren't a configuration's, and what is kept on other items)."""
	if source is None:
		return None
	item = source.get("ItemType") or ""
	if item in NOT_SETTINGS or source.get("Label") == "Globals":
		return None
	if item:
		return item
	return "Configuration" if source.get("Label") == "Configuration" else ""


def rows(folder):
	out = []
	left = []
	seen = set()
	for file, page, program in RULES:
		root = ET.parse(os.path.join(folder, file)).getroot()
		ns = root.tag[:root.tag.index("}") + 1]
		rule_source = root.find(ns + "Rule.DataSource")
		rule_tool = kept(rule_source[0]) if rule_source is not None and len(rule_source) else None
		categories = {}
		for category in root.iter(ns + "Category"):
			categories[category.get("Name")] = category.get("DisplayName") or category.get("Name")
		for element in root:
			kind = element.tag.replace(ns, "")
			if kind not in KINDS:
				continue
			name = element.get("Name")
			own = element.find(ns + kind + ".DataSource")
			tool = kept(own[0]) if own is not None and len(own) else rule_tool
			if own is not None and len(own) and own[0].get("PersistedName"):
				name = own[0].get("PersistedName")
			if element.get("Visible") == "false" or tool is None or not element.get("DisplayName"):
				left.append("%s.%s" % (tool or page, element.get("Name")))
				continue
			if (tool, name) in seen:
				continue		# Said by two pages (the language standard: the project's page and the compiler's).
			seen.add((tool, name))
			category = categories.get(element.get("Category"), element.get("Category") or "General")
			label = element.get("DisplayName")
			if own is not None and len(own) and own[0].get("PersistedName"):
				label = "%s: %s" % (own[0].get("ItemType"), label)		# "PreBuildEvent: Command Line"
			adds, separator, choices = "false", "0", "NULL"
			if kind == "StringListProperty" or name == "AdditionalOptions":
				adds = "true"
				separator = "' '" if name == "AdditionalOptions" else "';'"
			elif kind == "BoolProperty":
				choices = quoted("false;true")
			elif kind == "EnumProperty":
				choices = quoted(";".join(value.get("Name") for value in element.findall(ns + "EnumValue")))
			section = page if len(categories) <= 3 else "%s: %s" % (page, category)
			out.append('\t{ %s, %s, %s, %s, false, "", BOK_MSVC, %s, %s, %s, %s },' % (quoted(tool), quoted(name), quoted(label), "true" if program else "false", adds, separator, choices, quoted(section)))
	return out, left


def main(arguments):
	if len(arguments) != 1:
		print(__doc__)
		return 1
	out, left = rows(arguments[0])
	print("//Written by MakeMSVCOptions.py from Visual Studio's descriptions of its property pages: %d settings.  Not by hand." % len(out))
	print("//Left out, each not shown by the pages, one of the project's globals, or kept on a reference:")
	for line in textwrap.wrap(", ".join(left) + ".", 112, break_long_words=False):
		print("//  " + line)
	print("\n".join(out))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
