/*
 * Copyright (C) 2006 James Hawkins
 *
 * A test program for installing MSI products.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define _WIN32_MSI 300
#define COBJMACROS

#include <stdio.h>

#include <windows.h>
#include <msiquery.h>
#include <msidefs.h>
#include <msi.h>
#include <fci.h>
#include <objidl.h>
#include <srrestoreptapi.h>
#include <shlobj.h>

#include "wine/test.h"

static UINT (WINAPI *pMsiQueryComponentStateA)
    (LPCSTR, LPCSTR, MSIINSTALLCONTEXT, LPCSTR, INSTALLSTATE*);
static UINT (WINAPI *pMsiSetExternalUIRecord)
    (INSTALLUI_HANDLER_RECORD, DWORD, LPVOID, PINSTALLUI_HANDLER_RECORD);
static UINT (WINAPI *pMsiSourceListEnumSourcesA)
    (LPCSTR, LPCSTR, MSIINSTALLCONTEXT, DWORD, DWORD, LPSTR, LPDWORD);
static UINT (WINAPI *pMsiSourceListGetInfoA)
    (LPCSTR, LPCSTR, MSIINSTALLCONTEXT, DWORD, LPCSTR, LPSTR, LPDWORD);

static BOOL (WINAPI *pConvertSidToStringSidA)(PSID, LPSTR*);
static BOOL (WINAPI *pGetTokenInformation)( HANDLE, TOKEN_INFORMATION_CLASS, LPVOID, DWORD, PDWORD );
static BOOL (WINAPI *pOpenProcessToken)( HANDLE, DWORD, PHANDLE );
static LONG (WINAPI *pRegDeleteKeyExA)(HKEY, LPCSTR, REGSAM, DWORD);
static BOOL (WINAPI *pIsWow64Process)(HANDLE, PBOOL);

static HMODULE hsrclient = 0;
static BOOL (WINAPI *pSRRemoveRestorePoint)(DWORD);
static BOOL (WINAPI *pSRSetRestorePointA)(RESTOREPOINTINFOA*, STATEMGRSTATUS*);

static BOOL on_win9x = FALSE;

static const char *msifile = "msitest.msi";
static const char *msifile2 = "winetest2.msi";
static const char *mstfile = "winetest.mst";
static CHAR CURR_DIR[MAX_PATH];
static CHAR PROG_FILES_DIR[MAX_PATH];
static CHAR COMMON_FILES_DIR[MAX_PATH];
static CHAR APP_DATA_DIR[MAX_PATH];
static CHAR WINDOWS_DIR[MAX_PATH];

/* msi database data */

static const CHAR component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                    "s72\tS38\ts72\ti2\tS255\tS72\n"
                                    "Component\tComponent\n"
                                    "Five\t{8CC92E9D-14B2-4CA4-B2AA-B11D02078087}\tNEWDIR\t2\t\tfive.txt\n"
                                    "Four\t{FD37B4EA-7209-45C0-8917-535F35A2F080}\tCABOUTDIR\t2\t\tfour.txt\n"
                                    "One\t{783B242E-E185-4A56-AF86-C09815EC053C}\tMSITESTDIR\t2\tNOT REINSTALL\tone.txt\n"
                                    "Three\t{010B6ADD-B27D-4EDD-9B3D-34C4F7D61684}\tCHANGEDDIR\t2\t\tthree.txt\n"
                                    "Two\t{BF03D1A6-20DA-4A65-82F3-6CAC995915CE}\tFIRSTDIR\t2\t\ttwo.txt\n"
                                    "dangler\t{6091DF25-EF96-45F1-B8E9-A9B1420C7A3C}\tTARGETDIR\t4\t\tregdata\n"
                                    "component\t\tMSITESTDIR\t0\t1\tfile\n"
                                    "service_comp\t\tMSITESTDIR\t0\t1\tservice_file";

static const CHAR directory_dat[] = "Directory\tDirectory_Parent\tDefaultDir\n"
                                    "s72\tS72\tl255\n"
                                    "Directory\tDirectory\n"
                                    "CABOUTDIR\tMSITESTDIR\tcabout\n"
                                    "CHANGEDDIR\tMSITESTDIR\tchanged:second\n"
                                    "FIRSTDIR\tMSITESTDIR\tfirst\n"
                                    "MSITESTDIR\tProgramFilesFolder\tmsitest\n"
                                    "NEWDIR\tCABOUTDIR\tnew\n"
                                    "ProgramFilesFolder\tTARGETDIR\t.\n"
                                    "TARGETDIR\t\tSourceDir";

static const CHAR feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                  "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                  "Feature\tFeature\n"
                                  "Five\t\tFive\tThe Five Feature\t5\t3\tNEWDIR\t0\n"
                                  "Four\t\tFour\tThe Four Feature\t4\t3\tCABOUTDIR\t0\n"
                                  "One\t\tOne\tThe One Feature\t1\t3\tMSITESTDIR\t0\n"
                                  "Three\t\tThree\tThe Three Feature\t3\t3\tCHANGEDDIR\t0\n"
                                  "Two\t\tTwo\tThe Two Feature\t2\t3\tFIRSTDIR\t0\n"
                                  "feature\t\t\t\t2\t1\tTARGETDIR\t0\n"
                                  "service_feature\t\t\t\t2\t1\tTARGETDIR\t0";

static const CHAR feature_comp_dat[] = "Feature_\tComponent_\n"
                                       "s38\ts72\n"
                                       "FeatureComponents\tFeature_\tComponent_\n"
                                       "Five\tFive\n"
                                       "Four\tFour\n"
                                       "One\tOne\n"
                                       "Three\tThree\n"
                                       "Two\tTwo\n"
                                       "feature\tcomponent\n"
                                       "service_feature\tservice_comp\n";

static const CHAR file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                               "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                               "File\tFile\n"
                               "five.txt\tFive\tfive.txt\t1000\t\t\t16384\t5\n"
                               "four.txt\tFour\tfour.txt\t1000\t\t\t16384\t4\n"
                               "one.txt\tOne\tone.txt\t1000\t\t\t0\t1\n"
                               "three.txt\tThree\tthree.txt\t1000\t\t\t0\t3\n"
                               "two.txt\tTwo\ttwo.txt\t1000\t\t\t0\t2\n"
                               "file\tcomponent\tfilename\t100\t\t\t8192\t1\n"
                               "service_file\tservice_comp\tservice.exe\t100\t\t\t8192\t1";

static const CHAR install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                           "s72\tS255\tI2\n"
                                           "InstallExecuteSequence\tAction\n"
                                           "AllocateRegistrySpace\tNOT Installed\t1550\n"
                                           "CostFinalize\t\t1000\n"
                                           "CostInitialize\t\t800\n"
                                           "FileCost\t\t900\n"
                                           "ResolveSource\t\t950\n"
                                           "MoveFiles\t\t1700\n"
                                           "InstallFiles\t\t4000\n"
                                           "DuplicateFiles\t\t4500\n"
                                           "WriteEnvironmentStrings\t\t4550\n"
                                           "CreateShortcuts\t\t4600\n"
                                           "InstallServices\t\t5000\n"
                                           "InstallFinalize\t\t6600\n"
                                           "InstallInitialize\t\t1500\n"
                                           "InstallValidate\t\t1400\n"
                                           "LaunchConditions\t\t100\n"
                                           "WriteRegistryValues\tSourceDir And SOURCEDIR\t5000";

static const CHAR media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                "i2\ti4\tL64\tS255\tS32\tS72\n"
                                "Media\tDiskId\n"
                                "1\t3\t\t\tDISK1\t\n"
                                "2\t5\t\tmsitest.cab\tDISK2\t\n";

static const CHAR property_dat[] = "Property\tValue\n"
                                   "s72\tl0\n"
                                   "Property\tProperty\n"
                                   "DefaultUIFont\tDlgFont8\n"
                                   "HASUIRUN\t0\n"
                                   "INSTALLLEVEL\t3\n"
                                   "InstallMode\tTypical\n"
                                   "Manufacturer\tWine\n"
                                   "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                   "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                   "ProductID\tnone\n"
                                   "ProductLanguage\t1033\n"
                                   "ProductName\tMSITEST\n"
                                   "ProductVersion\t1.1.1\n"
                                   "PROMPTROLLBACKCOST\tP\n"
                                   "Setup\tSetup\n"
                                   "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                   "AdminProperties\tPOSTADMIN\n"
                                   "ROOTDRIVE\tC:\\\n"
                                   "SERVNAME\tTestService\n"
                                   "SERVDISP\tTestServiceDisp\n";

static const CHAR aup_property_dat[] = "Property\tValue\n"
                                       "s72\tl0\n"
                                       "Property\tProperty\n"
                                       "DefaultUIFont\tDlgFont8\n"
                                       "HASUIRUN\t0\n"
                                       "ALLUSERS\t1\n"
                                       "INSTALLLEVEL\t3\n"
                                       "InstallMode\tTypical\n"
                                       "Manufacturer\tWine\n"
                                       "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                       "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                       "ProductID\tnone\n"
                                       "ProductLanguage\t1033\n"
                                       "ProductName\tMSITEST\n"
                                       "ProductVersion\t1.1.1\n"
                                       "PROMPTROLLBACKCOST\tP\n"
                                       "Setup\tSetup\n"
                                       "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                       "AdminProperties\tPOSTADMIN\n"
                                       "ROOTDRIVE\tC:\\\n"
                                       "SERVNAME\tTestService\n"
                                       "SERVDISP\tTestServiceDisp\n";

static const CHAR aup2_property_dat[] = "Property\tValue\n"
                                        "s72\tl0\n"
                                        "Property\tProperty\n"
                                        "DefaultUIFont\tDlgFont8\n"
                                        "HASUIRUN\t0\n"
                                        "ALLUSERS\t2\n"
                                        "INSTALLLEVEL\t3\n"
                                        "InstallMode\tTypical\n"
                                        "Manufacturer\tWine\n"
                                        "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                        "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                        "ProductID\tnone\n"
                                        "ProductLanguage\t1033\n"
                                        "ProductName\tMSITEST\n"
                                        "ProductVersion\t1.1.1\n"
                                        "PROMPTROLLBACKCOST\tP\n"
                                        "Setup\tSetup\n"
                                        "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                        "AdminProperties\tPOSTADMIN\n"
                                        "ROOTDRIVE\tC:\\\n"
                                        "SERVNAME\tTestService\n"
                                        "SERVDISP\tTestServiceDisp\n";

static const CHAR icon_property_dat[] = "Property\tValue\n"
                                        "s72\tl0\n"
                                        "Property\tProperty\n"
                                        "DefaultUIFont\tDlgFont8\n"
                                        "HASUIRUN\t0\n"
                                        "INSTALLLEVEL\t3\n"
                                        "InstallMode\tTypical\n"
                                        "Manufacturer\tWine\n"
                                        "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                        "ProductCode\t{7DF88A49-996F-4EC8-A022-BF956F9B2CBB}\n"
                                        "ProductID\tnone\n"
                                        "ProductLanguage\t1033\n"
                                        "ProductName\tMSITEST\n"
                                        "ProductVersion\t1.1.1\n"
                                        "PROMPTROLLBACKCOST\tP\n"
                                        "Setup\tSetup\n"
                                        "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                        "AdminProperties\tPOSTADMIN\n"
                                        "ROOTDRIVE\tC:\\\n"
                                        "SERVNAME\tTestService\n"
                                        "SERVDISP\tTestServiceDisp\n";

static const CHAR shortcut_dat[] = "Shortcut\tDirectory_\tName\tComponent_\tTarget\tArguments\tDescription\tHotkey\tIcon_\tIconIndex\tShowCmd\tWkDir\n"
                                   "s72\ts72\tl128\ts72\ts72\tS255\tL255\tI2\tS72\tI2\tI2\tS72\n"
                                   "Shortcut\tShortcut\n"
                                   "Shortcut\tMSITESTDIR\tShortcut\tcomponent\tShortcut\t\tShortcut\t\t\t\t\t\n";

static const CHAR environment_dat[] = "Environment\tName\tValue\tComponent_\n"
                                      "s72\tl255\tL255\ts72\n"
                                      "Environment\tEnvironment\n"
                                      "Var1\t=-MSITESTVAR1\t1\tOne\n"
                                      "Var2\tMSITESTVAR2\t1\tOne\n"
                                      "Var3\t=-MSITESTVAR3\t1\tOne\n"
                                      "Var4\tMSITESTVAR4\t1\tOne\n"
                                      "Var5\t-MSITESTVAR5\t\tOne\n"
                                      "Var6\tMSITESTVAR6\t\tOne\n"
                                      "Var7\t!-MSITESTVAR7\t\tOne\n"
                                      "Var8\t!-*MSITESTVAR8\t\tOne\n"
                                      "Var9\t=-MSITESTVAR9\t\tOne\n"
                                      "Var10\t=MSITESTVAR10\t\tOne\n"
                                      "Var11\t+-MSITESTVAR11\t[~];1\tOne\n"
                                      "Var12\t+-MSITESTVAR11\t[~];2\tOne\n"
                                      "Var13\t+-MSITESTVAR12\t[~];1\tOne\n"
                                      "Var14\t=MSITESTVAR13\t[~];1\tOne\n"
                                      "Var15\t=MSITESTVAR13\t[~];2\tOne\n"
                                      "Var16\t=MSITESTVAR14\t;1;\tOne\n"
                                      "Var17\t=MSITESTVAR15\t;;1;;\tOne\n"
                                      "Var18\t=MSITESTVAR16\t 1 \tOne\n"
                                      "Var19\t+-MSITESTVAR17\t1\tOne\n"
                                      "Var20\t+-MSITESTVAR17\t;;2;;[~]\tOne\n"
                                      "Var21\t+-MSITESTVAR18\t1\tOne\n"
                                      "Var22\t+-MSITESTVAR18\t[~];;2;;\tOne\n"
                                      "Var23\t+-MSITESTVAR19\t1\tOne\n"
                                      "Var24\t+-MSITESTVAR19\t[~]2\tOne\n"
                                      "Var25\t+-MSITESTVAR20\t1\tOne\n"
                                      "Var26\t+-MSITESTVAR20\t2[~]\tOne\n";

/* Expected results, starting from MSITESTVAR11 onwards */
static const CHAR *environment_dat_results[] = {"1;2",    /*MSITESTVAR11*/
                                                "1",      /*MSITESTVAR12*/
                                                "1;2",    /*MSITESTVAR13*/
                                                ";1;",    /*MSITESTVAR14*/
                                                ";;1;;",  /*MSITESTVAR15*/
                                                " 1 ",    /*MSITESTVAR16*/
                                                ";;2;;1", /*MSITESTVAR17*/
                                                "1;;2;;", /*MSITESTVAR18*/
                                                "1",      /*MSITESTVAR19*/
                                                "1",      /*MSITESTVAR20*/
                                                NULL};

static const CHAR condition_dat[] = "Feature_\tLevel\tCondition\n"
                                    "s38\ti2\tS255\n"
                                    "Condition\tFeature_\tLevel\n"
                                    "One\t4\t1\n";

static const CHAR up_property_dat[] = "Property\tValue\n"
                                      "s72\tl0\n"
                                      "Property\tProperty\n"
                                      "DefaultUIFont\tDlgFont8\n"
                                      "HASUIRUN\t0\n"
                                      "INSTALLLEVEL\t3\n"
                                      "InstallMode\tTypical\n"
                                      "Manufacturer\tWine\n"
                                      "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                      "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                      "ProductID\tnone\n"
                                      "ProductLanguage\t1033\n"
                                      "ProductName\tMSITEST\n"
                                      "ProductVersion\t1.1.1\n"
                                      "PROMPTROLLBACKCOST\tP\n"
                                      "Setup\tSetup\n"
                                      "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                      "AdminProperties\tPOSTADMIN\n"
                                      "ROOTDRIVE\tC:\\\n"
                                      "SERVNAME\tTestService\n"
                                      "SERVDISP\tTestServiceDisp\n"
                                      "RemovePreviousVersions\t1\n";

static const CHAR up2_property_dat[] = "Property\tValue\n"
                                       "s72\tl0\n"
                                       "Property\tProperty\n"
                                       "DefaultUIFont\tDlgFont8\n"
                                       "HASUIRUN\t0\n"
                                       "INSTALLLEVEL\t3\n"
                                       "InstallMode\tTypical\n"
                                       "Manufacturer\tWine\n"
                                       "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                       "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                       "ProductID\tnone\n"
                                       "ProductLanguage\t1033\n"
                                       "ProductName\tMSITEST\n"
                                       "ProductVersion\t1.1.2\n"
                                       "PROMPTROLLBACKCOST\tP\n"
                                       "Setup\tSetup\n"
                                       "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                       "AdminProperties\tPOSTADMIN\n"
                                       "ROOTDRIVE\tC:\\\n"
                                       "SERVNAME\tTestService\n"
                                       "SERVDISP\tTestServiceDisp\n";

static const CHAR up3_property_dat[] = "Property\tValue\n"
                                       "s72\tl0\n"
                                       "Property\tProperty\n"
                                       "DefaultUIFont\tDlgFont8\n"
                                       "HASUIRUN\t0\n"
                                       "INSTALLLEVEL\t3\n"
                                       "InstallMode\tTypical\n"
                                       "Manufacturer\tWine\n"
                                       "PIDTemplate\t12345<###-%%%%%%%>@@@@@\n"
                                       "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                       "ProductID\tnone\n"
                                       "ProductLanguage\t1033\n"
                                       "ProductName\tMSITEST\n"
                                       "ProductVersion\t1.1.2\n"
                                       "PROMPTROLLBACKCOST\tP\n"
                                       "Setup\tSetup\n"
                                       "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n"
                                       "AdminProperties\tPOSTADMIN\n"
                                       "ROOTDRIVE\tC:\\\n"
                                       "SERVNAME\tTestService\n"
                                       "SERVDISP\tTestServiceDisp\n"
                                       "RemovePreviousVersions\t1\n";

static const CHAR registry_dat[] = "Registry\tRoot\tKey\tName\tValue\tComponent_\n"
                                   "s72\ti2\tl255\tL255\tL0\ts72\n"
                                   "Registry\tRegistry\n"
                                   "Apples\t2\tSOFTWARE\\Wine\\msitest\tName\timaname\tOne\n"
                                   "Oranges\t2\tSOFTWARE\\Wine\\msitest\tnumber\t#314\tTwo\n"
                                   "regdata\t2\tSOFTWARE\\Wine\\msitest\tblah\tbad\tdangler\n"
                                   "OrderTest\t2\tSOFTWARE\\Wine\\msitest\tOrderTestName\tOrderTestValue\tcomponent";

static const CHAR service_install_dat[] = "ServiceInstall\tName\tDisplayName\tServiceType\tStartType\tErrorControl\t"
                                          "LoadOrderGroup\tDependencies\tStartName\tPassword\tArguments\tComponent_\tDescription\n"
                                          "s72\ts255\tL255\ti4\ti4\ti4\tS255\tS255\tS255\tS255\tS255\ts72\tL255\n"
                                          "ServiceInstall\tServiceInstall\n"
                                          "TestService\t[SERVNAME]\t[SERVDISP]\t2\t3\t0\t\t\tTestService\t\t\tservice_comp\t\t";

static const CHAR service_control_dat[] = "ServiceControl\tName\tEvent\tArguments\tWait\tComponent_\n"
                                          "s72\tl255\ti2\tL255\tI2\ts72\n"
                                          "ServiceControl\tServiceControl\n"
                                          "ServiceControl\tTestService\t8\t\t0\tservice_comp";

static const CHAR sss_service_control_dat[] = "ServiceControl\tName\tEvent\tArguments\tWait\tComponent_\n"
                                              "s72\tl255\ti2\tL255\tI2\ts72\n"
                                              "ServiceControl\tServiceControl\n"
                                              "ServiceControl\tSpooler\t1\t\t0\tservice_comp";

static const CHAR sss_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "ResolveSource\t\t950\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "DeleteServices\t\t5000\n"
                                               "MoveFiles\t\t5100\n"
                                               "InstallFiles\t\t5200\n"
                                               "DuplicateFiles\t\t5300\n"
                                               "StartServices\t\t5400\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR sds_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "ResolveSource\t\t950\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "DeleteServices\tInstalled\t5000\n"
                                               "MoveFiles\t\t5100\n"
                                               "InstallFiles\t\t5200\n"
                                               "DuplicateFiles\t\t5300\n"
                                               "InstallServices\tNOT Installed\t5400\n"
                                               "RegisterProduct\t\t5500\n"
                                               "PublishFeatures\t\t5600\n"
                                               "PublishProduct\t\t5700\n"
                                               "InstallFinalize\t\t6000\n";

/* tables for test_continuouscabs */
static const CHAR cc_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "maximus\t\tMSITESTDIR\t0\t1\tmaximus\n"
                                       "augustus\t\tMSITESTDIR\t0\t1\taugustus\n"
                                       "caesar\t\tMSITESTDIR\t0\t1\tcaesar\n";

static const CHAR cc2_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "maximus\t\tMSITESTDIR\t0\t1\tmaximus\n"
                                        "augustus\t\tMSITESTDIR\t0\t0\taugustus\n"
                                        "caesar\t\tMSITESTDIR\t0\t1\tcaesar\n";

static const CHAR cc_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                     "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                     "Feature\tFeature\n"
                                     "feature\t\t\t\t2\t1\tTARGETDIR\t0";

static const CHAR cc_feature_comp_dat[] = "Feature_\tComponent_\n"
                                          "s38\ts72\n"
                                          "FeatureComponents\tFeature_\tComponent_\n"
                                          "feature\tmaximus\n"
                                          "feature\taugustus\n"
                                          "feature\tcaesar";

static const CHAR cc_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "maximus\tmaximus\tmaximus\t500\t\t\t16384\t1\n"
                                  "augustus\taugustus\taugustus\t50000\t\t\t16384\t2\n"
                                  "caesar\tcaesar\tcaesar\t500\t\t\t16384\t12";

static const CHAR cc2_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "maximus\tmaximus\tmaximus\t500\t\t\t16384\t1\n"
                                   "augustus\taugustus\taugustus\t50000\t\t\t16384\t2\n"
                                   "tiberius\tmaximus\ttiberius\t500\t\t\t16384\t3\n"
                                   "caesar\tcaesar\tcaesar\t500\t\t\t16384\t12";

static const CHAR cc_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                   "i2\ti4\tL64\tS255\tS32\tS72\n"
                                   "Media\tDiskId\n"
                                   "1\t10\t\ttest1.cab\tDISK1\t\n"
                                   "2\t2\t\ttest2.cab\tDISK2\t\n"
                                   "3\t12\t\ttest3.cab\tDISK3\t\n";

static const CHAR co_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "maximus\tmaximus\tmaximus\t500\t\t\t16384\t1\n"
                                  "augustus\taugustus\taugustus\t50000\t\t\t16384\t2\n"
                                  "caesar\tcaesar\tcaesar\t500\t\t\t16384\t3";

static const CHAR co_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                   "i2\ti4\tL64\tS255\tS32\tS72\n"
                                   "Media\tDiskId\n"
                                   "1\t10\t\ttest1.cab\tDISK1\t\n"
                                   "2\t2\t\ttest2.cab\tDISK2\t\n"
                                   "3\t3\t\ttest3.cab\tDISK3\t\n";

static const CHAR co2_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                    "i2\ti4\tL64\tS255\tS32\tS72\n"
                                    "Media\tDiskId\n"
                                    "1\t10\t\ttest1.cab\tDISK1\t\n"
                                    "2\t12\t\ttest3.cab\tDISK3\t\n"
                                    "3\t2\t\ttest2.cab\tDISK2\t\n";

static const CHAR mm_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "maximus\tmaximus\tmaximus\t500\t\t\t512\t1\n"
                                  "augustus\taugustus\taugustus\t500\t\t\t512\t2\n"
                                  "caesar\tcaesar\tcaesar\t500\t\t\t16384\t3";

static const CHAR mm_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                   "i2\ti4\tL64\tS255\tS32\tS72\n"
                                   "Media\tDiskId\n"
                                   "1\t3\t\ttest1.cab\tDISK1\t\n";

static const CHAR ss_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                   "i2\ti4\tL64\tS255\tS32\tS72\n"
                                   "Media\tDiskId\n"
                                   "1\t2\t\ttest1.cab\tDISK1\t\n"
                                   "2\t2\t\ttest2.cab\tDISK2\t\n"
                                   "3\t12\t\ttest3.cab\tDISK3\t\n";

/* tables for test_uiLevelFlags */
static const CHAR ui_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "maximus\t\tMSITESTDIR\t0\tHASUIRUN=1\tmaximus\n"
                                       "augustus\t\tMSITESTDIR\t0\t1\taugustus\n"
                                       "caesar\t\tMSITESTDIR\t0\t1\tcaesar\n";

static const CHAR ui_install_ui_seq_dat[] = "Action\tCondition\tSequence\n"
                                           "s72\tS255\tI2\n"
                                           "InstallUISequence\tAction\n"
                                           "SetUIProperty\t\t5\n"
                                           "ExecuteAction\t\t1100\n";

static const CHAR ui_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                           "s72\ti2\tS64\tS0\tS255\n"
                                           "CustomAction\tAction\n"
                                           "SetUIProperty\t51\tHASUIRUN\t1\t\n";

static const CHAR rof_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "maximus\t\tMSITESTDIR\t0\t1\tmaximus\n";

static const CHAR rof_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "feature\t\tFeature\tFeature\t2\t1\tTARGETDIR\t0\n"
                                      "montecristo\t\tFeature\tFeature\t2\t1\tTARGETDIR\t0";

static const CHAR rof_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "feature\tmaximus\n"
                                           "montecristo\tmaximus";

static const CHAR rof_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "maximus\tmaximus\tmaximus\t500\t\t\t8192\t1";

static const CHAR rof_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                    "i2\ti4\tL64\tS255\tS32\tS72\n"
                                    "Media\tDiskId\n"
                                    "1\t1\t\t\tDISK1\t\n";

static const CHAR rofc_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                    "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                    "File\tFile\n"
                                    "maximus\tmaximus\tmaximus\t500\t\t\t16384\t1";

static const CHAR rofc_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                     "i2\ti4\tL64\tS255\tS32\tS72\n"
                                     "Media\tDiskId\n"
                                     "1\t1\t\ttest1.cab\tDISK1\t\n";

static const CHAR lus2_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                     "i2\ti4\tL64\tS255\tS32\tS72\n"
                                     "Media\tDiskId\n"
                                     "1\t1\t\t#test1.cab\tDISK1\t\n";

static const CHAR sdp_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "AllocateRegistrySpace\tNOT Installed\t1550\n"
                                               "CostFinalize\t\t1000\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "InstallFiles\t\t4000\n"
                                               "InstallFinalize\t\t6600\n"
                                               "InstallInitialize\t\t1500\n"
                                               "InstallValidate\t\t1400\n"
                                               "LaunchConditions\t\t100\n"
                                               "SetDirProperty\t\t950";

static const CHAR sdp_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                            "s72\ti2\tS64\tS0\tS255\n"
                                            "CustomAction\tAction\n"
                                            "SetDirProperty\t51\tMSITESTDIR\t[CommonFilesFolder]msitest\\\t\n";

static const CHAR cie_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "maximus\t\tMSITESTDIR\t0\t1\tmaximus\n"
                                        "augustus\t\tMSITESTDIR\t0\t1\taugustus\n"
                                        "caesar\t\tMSITESTDIR\t0\t1\tcaesar\n"
                                        "gaius\t\tMSITESTDIR\t0\t1\tgaius\n";

static const CHAR cie_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "feature\tmaximus\n"
                                           "feature\taugustus\n"
                                           "feature\tcaesar\n"
                                           "feature\tgaius";

static const CHAR cie_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "maximus\tmaximus\tmaximus\t500\t\t\t16384\t1\n"
                                   "augustus\taugustus\taugustus\t50000\t\t\t16384\t2\n"
                                   "caesar\tcaesar\tcaesar\t500\t\t\t16384\t12\n"
                                   "gaius\tgaius\tgaius\t500\t\t\t8192\t11";

static const CHAR cie_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                    "i2\ti4\tL64\tS255\tS32\tS72\n"
                                    "Media\tDiskId\n"
                                    "1\t1\t\ttest1.cab\tDISK1\t\n"
                                    "2\t2\t\ttest2.cab\tDISK2\t\n"
                                    "3\t12\t\ttest3.cab\tDISK3\t\n";

static const CHAR ci_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "CostFinalize\t\t1000\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "InstallFiles\t\t4000\n"
                                              "InstallServices\t\t5000\n"
                                              "InstallFinalize\t\t6600\n"
                                              "InstallInitialize\t\t1500\n"
                                              "RunInstall\t\t1600\n"
                                              "InstallValidate\t\t1400\n"
                                              "LaunchConditions\t\t100";

static const CHAR ci_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                            "s72\ti2\tS64\tS0\tS255\n"
                                            "CustomAction\tAction\n"
                                            "RunInstall\t87\tmsitest\\concurrent.msi\tMYPROP=[UILevel]\t\n";

static const CHAR ci_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "maximus\t{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}\tMSITESTDIR\t0\tUILevel=5\tmaximus\n";

static const CHAR ci2_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "augustus\t\tMSITESTDIR\t0\tUILevel=3 AND MYPROP=5\taugustus\n";

static const CHAR ci2_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "feature\taugustus";

static const CHAR ci2_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "augustus\taugustus\taugustus\t500\t\t\t8192\t1";

static const CHAR spf_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                            "s72\ti2\tS64\tS0\tS255\n"
                                            "CustomAction\tAction\n"
                                            "SetFolderProp\t51\tMSITESTDIR\t[ProgramFilesFolder]\\msitest\\added\t\n";

static const CHAR spf_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "CostFinalize\t\t1000\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "SetFolderProp\t\t950\n"
                                               "InstallFiles\t\t4000\n"
                                               "InstallServices\t\t5000\n"
                                               "InstallFinalize\t\t6600\n"
                                               "InstallInitialize\t\t1500\n"
                                               "InstallValidate\t\t1400\n"
                                               "LaunchConditions\t\t100";

static const CHAR spf_install_ui_seq_dat[] = "Action\tCondition\tSequence\n"
                                             "s72\tS255\tI2\n"
                                             "InstallUISequence\tAction\n"
                                             "CostInitialize\t\t800\n"
                                             "FileCost\t\t900\n"
                                             "CostFinalize\t\t1000\n"
                                             "ExecuteAction\t\t1100\n";

static const CHAR pp_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "ValidateProductID\t\t700\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "CostFinalize\t\t1000\n"
                                              "InstallValidate\t\t1400\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\tPROCESS_COMPONENTS=1 Or FULL=1\t1600\n"
                                              "UnpublishFeatures\tUNPUBLISH_FEATURES=1 Or FULL=1\t1800\n"
                                              "RemoveFiles\t\t3500\n"
                                              "InstallFiles\t\t4000\n"
                                              "RegisterUser\tREGISTER_USER=1 Or FULL=1\t6000\n"
                                              "RegisterProduct\tREGISTER_PRODUCT=1 Or FULL=1\t6100\n"
                                              "PublishFeatures\tPUBLISH_FEATURES=1 Or FULL=1\t6300\n"
                                              "PublishProduct\tPUBLISH_PRODUCT=1 Or FULL=1\t6400\n"
                                              "InstallFinalize\t\t6600";

static const CHAR ppc_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "maximus\t{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}\tMSITESTDIR\t0\tUILevel=5\tmaximus\n"
                                        "augustus\t{5AD3C142-CEF8-490D-B569-784D80670685}\tMSITESTDIR\t1\t\taugustus\n";

static const CHAR ppc_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "maximus\tmaximus\tmaximus\t500\t\t\t8192\t1\n"
                                   "augustus\taugustus\taugustus\t500\t\t\t8192\t2";

static const CHAR ppc_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                    "i2\ti4\tL64\tS255\tS32\tS72\n"
                                    "Media\tDiskId\n"
                                    "1\t2\t\t\tDISK1\t\n";

static const CHAR ppc_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "feature\tmaximus\n"
                                           "feature\taugustus\n"
                                           "montecristo\tmaximus";

static const CHAR tp_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "augustus\t\tMSITESTDIR\t0\tprop=\"val\"\taugustus\n";

static const CHAR cwd_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "augustus\t\tMSITESTDIR\t0\t\taugustus\n";

static const CHAR adm_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "augustus\t\tMSITESTDIR\t0\tPOSTADMIN=1\taugustus";

static const CHAR adm_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                            "s72\ti2\tS64\tS0\tS255\n"
                                            "CustomAction\tAction\n"
                                            "SetPOSTADMIN\t51\tPOSTADMIN\t1\t\n";

static const CHAR adm_admin_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                             "s72\tS255\tI2\n"
                                             "AdminExecuteSequence\tAction\n"
                                             "CostFinalize\t\t1000\n"
                                             "CostInitialize\t\t800\n"
                                             "FileCost\t\t900\n"
                                             "SetPOSTADMIN\t\t950\n"
                                             "InstallFiles\t\t4000\n"
                                             "InstallFinalize\t\t6600\n"
                                             "InstallInitialize\t\t1500\n"
                                             "InstallValidate\t\t1400\n"
                                             "LaunchConditions\t\t100";

static const CHAR amp_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "augustus\t\tMSITESTDIR\t0\tMYPROP=2718 and MyProp=42\taugustus\n";

static const CHAR rem_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "hydrogen\t{C844BD1E-1907-4C00-8BC9-150BD70DF0A1}\tMSITESTDIR\t0\t\thydrogen\n"
                                        "helium\t{5AD3C142-CEF8-490D-B569-784D80670685}\tMSITESTDIR\t1\t\thelium\n"
                                        "lithium\t\tMSITESTDIR\t2\t\tlithium\n";

static const CHAR rem_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "feature\thydrogen\n"
                                           "feature\thelium\n"
                                           "feature\tlithium";

static const CHAR rem_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "hydrogen\thydrogen\thydrogen\t0\t\t\t8192\t1\n"
                                   "helium\thelium\thelium\t0\t\t\t8192\t1\n"
                                   "lithium\tlithium\tlithium\t0\t\t\t8192\t1";

static const CHAR rem_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "ValidateProductID\t\t700\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "UnpublishFeatures\t\t1800\n"
                                               "RemoveFiles\t\t3500\n"
                                               "InstallFiles\t\t4000\n"
                                               "RegisterProduct\t\t6100\n"
                                               "PublishFeatures\t\t6300\n"
                                               "PublishProduct\t\t6400\n"
                                               "InstallFinalize\t\t6600";

static const CHAR rem_remove_files_dat[] = "FileKey\tComponent_\tFileName\tDirProperty\tInstallMode\n"
                                           "s72\ts72\tS255\ts72\tI2\n"
                                           "RemoveFile\tFileKey\n"
                                           "furlong\thydrogen\tfurlong\tMSITESTDIR\t1\n"
                                           "firkin\thelium\tfirkin\tMSITESTDIR\t1\n"
                                           "fortnight\tlithium\tfortnight\tMSITESTDIR\t1\n"
                                           "becquerel\thydrogen\tbecquerel\tMSITESTDIR\t2\n"
                                           "dioptre\thelium\tdioptre\tMSITESTDIR\t2\n"
                                           "attoparsec\tlithium\tattoparsec\tMSITESTDIR\t2\n"
                                           "storeys\thydrogen\tstoreys\tMSITESTDIR\t3\n"
                                           "block\thelium\tblock\tMSITESTDIR\t3\n"
                                           "siriometer\tlithium\tsiriometer\tMSITESTDIR\t3\n"
                                           "nanoacre\thydrogen\t\tCABOUTDIR\t3\n";

static const CHAR mov_move_file_dat[] = "FileKey\tComponent_\tSourceName\tDestName\tSourceFolder\tDestFolder\tOptions\n"
                                        "s72\ts72\tS255\tS255\tS72\ts72\ti2\n"
                                        "MoveFile\tFileKey\n"
                                        "abkhazia\taugustus\tnonexistent\tdest\tSourceDir\tMSITESTDIR\t0\n"
                                        "bahamas\taugustus\tnonexistent\tdest\tSourceDir\tMSITESTDIR\t1\n"
                                        "cambodia\taugustus\tcameroon\tcanada\tSourceDir\tMSITESTDIR\t0\n"
                                        "denmark\taugustus\tdjibouti\tdominica\tSourceDir\tMSITESTDIR\t1\n"
                                        "ecuador\taugustus\tegypt\telsalvador\tNotAProp\tMSITESTDIR\t1\n"
                                        "fiji\taugustus\tfinland\tfrance\tSourceDir\tNotAProp\t1\n"
                                        "gabon\taugustus\tgambia\tgeorgia\tSOURCEFULL\tMSITESTDIR\t1\n"
                                        "haiti\taugustus\thonduras\thungary\tSourceDir\tDESTFULL\t1\n"
                                        "iceland\taugustus\tindia\tindonesia\tMSITESTDIR\tMSITESTDIR\t1\n"
                                        "jamaica\taugustus\tjapan\tjordan\tFILEPATHBAD\tMSITESTDIR\t1\n"
                                        "kazakhstan\taugustus\t\tkiribati\tFILEPATHGOOD\tMSITESTDIR\t1\n"
                                        "laos\taugustus\tlatvia\tlebanon\tSourceDir\tMSITESTDIR\t1\n"
                                        "namibia\taugustus\tnauru\tkiribati\tSourceDir\tMSITESTDIR\t1\n"
                                        "pakistan\taugustus\tperu\tsfn|poland\tSourceDir\tMSITESTDIR\t1\n"
                                        "wildcard\taugustus\tapp*\twildcard\tSourceDir\tMSITESTDIR\t1\n"
                                        "single\taugustus\tf?o\tsingle\tSourceDir\tMSITESTDIR\t1\n"
                                        "wildcardnodest\taugustus\tbudd*\t\tSourceDir\tMSITESTDIR\t1\n"
                                        "singlenodest\taugustus\tb?r\t\tSourceDir\tMSITESTDIR\t1\n";

static const CHAR mc_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "maximus\t\tMSITESTDIR\t0\t1\tmaximus\n"
                                        "augustus\t\tMSITESTDIR\t0\t1\taugustus\n"
                                        "caesar\t\tMSITESTDIR\t0\t1\tcaesar\n"
                                        "gaius\t\tMSITESTDIR\t0\tGAIUS=1\tgaius\n";

static const CHAR mc_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "maximus\tmaximus\tmaximus\t500\t\t\t16384\t1\n"
                                  "augustus\taugustus\taugustus\t500\t\t\t0\t2\n"
                                  "caesar\tcaesar\tcaesar\t500\t\t\t16384\t3\n"
                                  "gaius\tgaius\tgaius\t500\t\t\t16384\t4";

static const CHAR mc_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                   "i2\ti4\tL64\tS255\tS32\tS72\n"
                                   "Media\tDiskId\n"
                                   "1\t1\t\ttest1.cab\tDISK1\t\n"
                                   "2\t2\t\ttest2.cab\tDISK2\t\n"
                                   "3\t3\t\ttest3.cab\tDISK3\t\n"
                                   "4\t4\t\ttest3.cab\tDISK3\t\n";

static const CHAR mc_file_hash_dat[] = "File_\tOptions\tHashPart1\tHashPart2\tHashPart3\tHashPart4\n"
                                       "s72\ti2\ti4\ti4\ti4\ti4\n"
                                       "MsiFileHash\tFile_\n"
                                       "caesar\t0\t850433704\t-241429251\t675791761\t-1221108824";

static const CHAR df_directory_dat[] = "Directory\tDirectory_Parent\tDefaultDir\n"
                                       "s72\tS72\tl255\n"
                                       "Directory\tDirectory\n"
                                       "THIS\tMSITESTDIR\tthis\n"
                                       "DOESNOT\tTHIS\tdoesnot\n"
                                       "NONEXISTENT\tDOESNOT\texist\n"
                                       "MSITESTDIR\tProgramFilesFolder\tmsitest\n"
                                       "ProgramFilesFolder\tTARGETDIR\t.\n"
                                       "TARGETDIR\t\tSourceDir";

static const CHAR df_duplicate_file_dat[] = "FileKey\tComponent_\tFile_\tDestName\tDestFolder\n"
                                            "s72\ts72\ts72\tS255\tS72\n"
                                            "DuplicateFile\tFileKey\n"
                                            "maximus\tmaximus\tmaximus\taugustus\t\n"
                                            "caesar\tmaximus\tmaximus\t\tNONEXISTENT\n"
                                            "augustus\tnosuchcomponent\tmaximus\t\tMSITESTDIR\n";

static const CHAR wrv_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "augustus\t\tMSITESTDIR\t0\t\taugustus\n";

static const CHAR wrv_registry_dat[] = "Registry\tRoot\tKey\tName\tValue\tComponent_\n"
                                       "s72\ti2\tl255\tL255\tL0\ts72\n"
                                       "Registry\tRegistry\n"
                                       "regdata\t2\tSOFTWARE\\Wine\\msitest\tValue\t[~]one[~]two[~]three\taugustus";

static const CHAR ca51_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                         "s72\tS38\ts72\ti2\tS255\tS72\n"
                                         "Component\tComponent\n"
                                         "augustus\t\tMSITESTDIR\t0\tMYPROP=42\taugustus\n";

static const CHAR ca51_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                                "s72\tS255\tI2\n"
                                                "InstallExecuteSequence\tAction\n"
                                                "ValidateProductID\t\t700\n"
                                                "GoodSetProperty\t\t725\n"
                                                "BadSetProperty\t\t750\n"
                                                "CostInitialize\t\t800\n"
                                                "ResolveSource\t\t810\n"
                                                "FileCost\t\t900\n"
                                                "SetSourceDir\tSRCDIR\t910\n"
                                                "CostFinalize\t\t1000\n"
                                                "InstallValidate\t\t1400\n"
                                                "InstallInitialize\t\t1500\n"
                                                "InstallFiles\t\t4000\n"
                                                "InstallFinalize\t\t6600";

static const CHAR ca51_custom_action_dat[] = "Action\tType\tSource\tTarget\n"
                                             "s72\ti2\tS64\tS0\n"
                                             "CustomAction\tAction\n"
                                             "GoodSetProperty\t51\tMYPROP\t42\n"
                                             "BadSetProperty\t51\t\tMYPROP\n"
                                             "SetSourceDir\t51\tSourceDir\t[SRCDIR]\n";

static const CHAR is_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                     "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                     "Feature\tFeature\n"
                                     "one\t\t\t\t2\t1\t\t0\n" /* favorLocal */
                                     "two\t\t\t\t2\t1\t\t1\n" /* favorSource */
                                     "three\t\t\t\t2\t1\t\t4\n" /* favorAdvertise */
                                     "four\t\t\t\t2\t0\t\t0"; /* disabled */

static const CHAR is_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "alpha\t\tMSITESTDIR\t0\t\talpha_file\n" /* favorLocal:Local */
                                       "beta\t\tMSITESTDIR\t1\t\tbeta_file\n" /* favorLocal:Source */
                                       "gamma\t\tMSITESTDIR\t2\t\tgamma_file\n" /* favorLocal:Optional */
                                       "theta\t\tMSITESTDIR\t0\t\ttheta_file\n" /* favorSource:Local */
                                       "delta\t\tMSITESTDIR\t1\t\tdelta_file\n" /* favorSource:Source */
                                       "epsilon\t\tMSITESTDIR\t2\t\tepsilon_file\n" /* favorSource:Optional */
                                       "zeta\t\tMSITESTDIR\t0\t\tzeta_file\n" /* favorAdvertise:Local */
                                       "iota\t\tMSITESTDIR\t1\t\tiota_file\n" /* favorAdvertise:Source */
                                       "eta\t\tMSITESTDIR\t2\t\teta_file\n" /* favorAdvertise:Optional */
                                       "kappa\t\tMSITESTDIR\t0\t\tkappa_file\n" /* disabled:Local */
                                       "lambda\t\tMSITESTDIR\t1\t\tlambda_file\n" /* disabled:Source */
                                       "mu\t\tMSITESTDIR\t2\t\tmu_file\n"; /* disabled:Optional */

static const CHAR is_feature_comp_dat[] = "Feature_\tComponent_\n"
                                          "s38\ts72\n"
                                          "FeatureComponents\tFeature_\tComponent_\n"
                                          "one\talpha\n"
                                          "one\tbeta\n"
                                          "one\tgamma\n"
                                          "two\ttheta\n"
                                          "two\tdelta\n"
                                          "two\tepsilon\n"
                                          "three\tzeta\n"
                                          "three\tiota\n"
                                          "three\teta\n"
                                          "four\tkappa\n"
                                          "four\tlambda\n"
                                          "four\tmu";

static const CHAR is_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "alpha_file\talpha\talpha\t500\t\t\t8192\t1\n"
                                  "beta_file\tbeta\tbeta\t500\t\t\t8291\t2\n"
                                  "gamma_file\tgamma\tgamma\t500\t\t\t8192\t3\n"
                                  "theta_file\ttheta\ttheta\t500\t\t\t8192\t4\n"
                                  "delta_file\tdelta\tdelta\t500\t\t\t8192\t5\n"
                                  "epsilon_file\tepsilon\tepsilon\t500\t\t\t8192\t6\n"
                                  "zeta_file\tzeta\tzeta\t500\t\t\t8192\t7\n"
                                  "iota_file\tiota\tiota\t500\t\t\t8192\t8\n"
                                  "eta_file\teta\teta\t500\t\t\t8192\t9\n"
                                  "kappa_file\tkappa\tkappa\t500\t\t\t8192\t10\n"
                                  "lambda_file\tlambda\tlambda\t500\t\t\t8192\t11\n"
                                  "mu_file\tmu\tmu\t500\t\t\t8192\t12";

static const CHAR is_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                   "i2\ti4\tL64\tS255\tS32\tS72\n"
                                   "Media\tDiskId\n"
                                   "1\t12\t\t\tDISK1\t\n";

static const CHAR sp_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "augustus\t\tTWODIR\t0\t\taugustus\n";

static const CHAR sp_directory_dat[] = "Directory\tDirectory_Parent\tDefaultDir\n"
                                       "s72\tS72\tl255\n"
                                       "Directory\tDirectory\n"
                                       "TARGETDIR\t\tSourceDir\n"
                                       "ProgramFilesFolder\tTARGETDIR\t.\n"
                                       "MSITESTDIR\tProgramFilesFolder\tmsitest:.\n"
                                       "ONEDIR\tMSITESTDIR\t.:shortone|longone\n"
                                       "TWODIR\tONEDIR\t.:shorttwo|longtwo";

static const CHAR mcp_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "hydrogen\t{C844BD1E-1907-4C00-8BC9-150BD70DF0A1}\tMSITESTDIR\t2\t\thydrogen\n"
                                        "helium\t{5AD3C142-CEF8-490D-B569-784D80670685}\tMSITESTDIR\t2\t\thelium\n"
                                        "lithium\t{4AF28FFC-71C7-4307-BDE4-B77C5338F56F}\tMSITESTDIR\t2\tPROPVAR=42\tlithium\n";

static const CHAR mcp_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "hydroxyl\t\thydroxyl\thydroxyl\t2\t1\tTARGETDIR\t0\n"
                                      "heliox\t\theliox\theliox\t2\t5\tTARGETDIR\t0\n"
                                      "lithia\t\tlithia\tlithia\t2\t10\tTARGETDIR\t0";

static const CHAR mcp_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "hydroxyl\thydrogen\n"
                                           "heliox\thelium\n"
                                           "lithia\tlithium";

static const CHAR mcomp_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                     "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                     "File\tFile\n"
                                     "hydrogen\thydrogen\thydrogen\t0\t\t\t8192\t1\n"
                                     "helium\thelium\thelium\t0\t\t\t8192\t1\n"
                                     "lithium\tlithium\tlithium\t0\t\t\t8192\t1\n"
                                     "beryllium\tmissingcomp\tberyllium\t0\t\t\t8192\t1";

static const CHAR ai_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "five.txt\tFive\tfive.txt\t1000\t\t\t16384\t5\n"
                                  "four.txt\tFour\tfour.txt\t1000\t\t\t16384\t4\n"
                                  "one.txt\tOne\tone.txt\t1000\t\t\t16384\t1\n"
                                  "three.txt\tThree\tthree.txt\t1000\t\t\t16384\t3\n"
                                  "two.txt\tTwo\ttwo.txt\t1000\t\t\t16384\t2\n"
                                  "file\tcomponent\tfilename\t100\t\t\t8192\t1\n"
                                  "service_file\tservice_comp\tservice.exe\t100\t\t\t8192\t1";

static const CHAR ip_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "CostFinalize\t\t1000\n"
                                              "ValidateProductID\t\t700\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "RemoveFiles\t\t3500\n"
                                              "InstallFiles\t\t4000\n"
                                              "RegisterUser\t\t6000\n"
                                              "RegisterProduct\t\t6100\n"
                                              "PublishFeatures\t\t6300\n"
                                              "PublishProduct\t\t6400\n"
                                              "InstallFinalize\t\t6600\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\t\t1600\n"
                                              "UnpublishFeatures\t\t1800\n"
                                              "InstallValidate\t\t1400\n"
                                              "LaunchConditions\t\t100\n"
                                              "TestInstalledProp\tInstalled AND NOT REMOVE\t950\n";

static const CHAR ip_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                           "s72\ti2\tS64\tS0\tS255\n"
                                           "CustomAction\tAction\n"
                                           "TestInstalledProp\t19\t\tTest failed\t\n";

static const CHAR aup_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "CostFinalize\t\t1000\n"
                                               "ValidateProductID\t\t700\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "RemoveFiles\t\t3500\n"
                                               "InstallFiles\t\t4000\n"
                                               "RegisterUser\t\t6000\n"
                                               "RegisterProduct\t\t6100\n"
                                               "PublishFeatures\t\t6300\n"
                                               "PublishProduct\t\t6400\n"
                                               "InstallFinalize\t\t6600\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "UnpublishFeatures\t\t1800\n"
                                               "InstallValidate\t\t1400\n"
                                               "LaunchConditions\t\t100\n"
                                               "TestAllUsersProp\tALLUSERS AND NOT REMOVE\t50\n";

static const CHAR aup2_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                                "s72\tS255\tI2\n"
                                                "InstallExecuteSequence\tAction\n"
                                                "CostFinalize\t\t1000\n"
                                                "ValidateProductID\t\t700\n"
                                                "CostInitialize\t\t800\n"
                                                "FileCost\t\t900\n"
                                                "RemoveFiles\t\t3500\n"
                                                "InstallFiles\t\t4000\n"
                                                "RegisterUser\t\t6000\n"
                                                "RegisterProduct\t\t6100\n"
                                                "PublishFeatures\t\t6300\n"
                                                "PublishProduct\t\t6400\n"
                                                "InstallFinalize\t\t6600\n"
                                                "InstallInitialize\t\t1500\n"
                                                "ProcessComponents\t\t1600\n"
                                                "UnpublishFeatures\t\t1800\n"
                                                "InstallValidate\t\t1400\n"
                                                "LaunchConditions\t\t100\n"
                                                "TestAllUsersProp\tALLUSERS=2 AND NOT REMOVE\t50\n";

static const CHAR aup3_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                                "s72\tS255\tI2\n"
                                                "InstallExecuteSequence\tAction\n"
                                                "CostFinalize\t\t1000\n"
                                                "ValidateProductID\t\t700\n"
                                                "CostInitialize\t\t800\n"
                                                "FileCost\t\t900\n"
                                                "RemoveFiles\t\t3500\n"
                                                "InstallFiles\t\t4000\n"
                                                "RegisterUser\t\t6000\n"
                                                "RegisterProduct\t\t6100\n"
                                                "PublishFeatures\t\t6300\n"
                                                "PublishProduct\t\t6400\n"
                                                "InstallFinalize\t\t6600\n"
                                                "InstallInitialize\t\t1500\n"
                                                "ProcessComponents\t\t1600\n"
                                                "UnpublishFeatures\t\t1800\n"
                                                "InstallValidate\t\t1400\n"
                                                "LaunchConditions\t\t100\n"
                                                "TestAllUsersProp\tALLUSERS=1 AND NOT REMOVE\t50\n";

static const CHAR aup_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                            "s72\ti2\tS64\tS0\tS255\n"
                                            "CustomAction\tAction\n"
                                            "TestAllUsersProp\t19\t\tTest failed\t\n";

static const CHAR cf_create_folders_dat[] = "Directory_\tComponent_\n"
                                            "s72\ts72\n"
                                            "CreateFolder\tDirectory_\tComponent_\n"
                                            "FIRSTDIR\tOne\n";

static const CHAR cf_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "CostFinalize\t\t1000\n"
                                              "ValidateProductID\t\t700\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "RemoveFiles\t\t3500\n"
                                              "CreateFolders\t\t3700\n"
                                              "InstallExecute\t\t3800\n"
                                              "TestCreateFolders\t\t3900\n"
                                              "InstallFiles\t\t4000\n"
                                              "RegisterUser\t\t6000\n"
                                              "RegisterProduct\t\t6100\n"
                                              "PublishFeatures\t\t6300\n"
                                              "PublishProduct\t\t6400\n"
                                              "InstallFinalize\t\t6600\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\t\t1600\n"
                                              "UnpublishFeatures\t\t1800\n"
                                              "InstallValidate\t\t1400\n"
                                              "LaunchConditions\t\t100\n";

static const CHAR cf_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                           "s72\ti2\tS64\tS0\tS255\n"
                                           "CustomAction\tAction\n"
                                           "TestCreateFolders\t19\t\tHalts installation\t\n";

static const CHAR rf_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "CostFinalize\t\t1000\n"
                                              "ValidateProductID\t\t700\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "RemoveFiles\t\t3500\n"
                                              "CreateFolders\t\t3600\n"
                                              "RemoveFolders\t\t3700\n"
                                              "InstallExecute\t\t3800\n"
                                              "TestCreateFolders\t\t3900\n"
                                              "InstallFiles\t\t4000\n"
                                              "RegisterUser\t\t6000\n"
                                              "RegisterProduct\t\t6100\n"
                                              "PublishFeatures\t\t6300\n"
                                              "PublishProduct\t\t6400\n"
                                              "InstallFinalize\t\t6600\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\t\t1600\n"
                                              "UnpublishFeatures\t\t1800\n"
                                              "InstallValidate\t\t1400\n"
                                              "LaunchConditions\t\t100\n";


static const CHAR sr_selfreg_dat[] = "File_\tCost\n"
                                     "s72\tI2\n"
                                     "SelfReg\tFile_\n"
                                     "one.txt\t1\n";

static const CHAR sr_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "CostFinalize\t\t1000\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "ResolveSource\t\t950\n"
                                              "MoveFiles\t\t1700\n"
                                              "SelfUnregModules\t\t3900\n"
                                              "InstallFiles\t\t4000\n"
                                              "DuplicateFiles\t\t4500\n"
                                              "WriteEnvironmentStrings\t\t4550\n"
                                              "CreateShortcuts\t\t4600\n"
                                              "InstallFinalize\t\t6600\n"
                                              "InstallInitialize\t\t1500\n"
                                              "InstallValidate\t\t1400\n"
                                              "LaunchConditions\t\t100\n";

static const CHAR font_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                     "i2\ti4\tL64\tS255\tS32\tS72\n"
                                     "Media\tDiskId\n"
                                     "1\t3\t\t\tDISK1\t\n";

static const CHAR font_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                    "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                    "File\tFile\n"
                                    "font.ttf\tfonts\tfont.ttf\t1000\t\t\t8192\t1\n";

static const CHAR font_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                       "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                       "Feature\tFeature\n"
                                       "fonts\t\t\tfont feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR font_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                         "s72\tS38\ts72\ti2\tS255\tS72\n"
                                         "Component\tComponent\n"
                                         "fonts\t{F5920ED0-1183-4B8F-9330-86CE56557C05}\tMSITESTDIR\t0\t\tfont.ttf\n";

static const CHAR font_feature_comp_dat[] = "Feature_\tComponent_\n"
                                            "s38\ts72\n"
                                            "FeatureComponents\tFeature_\tComponent_\n"
                                            "fonts\tfonts\n";

static const CHAR font_dat[] = "File_\tFontTitle\n"
                               "s72\tS128\n"
                               "Font\tFile_\n"
                               "font.ttf\tmsi test font\n";

static const CHAR font_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                                "s72\tS255\tI2\n"
                                                "InstallExecuteSequence\tAction\n"
                                                "ValidateProductID\t\t700\n"
                                                "CostInitialize\t\t800\n"
                                                "FileCost\t\t900\n"
                                                "CostFinalize\t\t1000\n"
                                                "InstallValidate\t\t1400\n"
                                                "InstallInitialize\t\t1500\n"
                                                "ProcessComponents\t\t1600\n"
                                                "UnpublishFeatures\t\t1800\n"
                                                "RemoveFiles\t\t3500\n"
                                                "InstallFiles\t\t4000\n"
                                                "RegisterFonts\t\t4100\n"
                                                "UnregisterFonts\t\t4200\n"
                                                "RegisterUser\t\t6000\n"
                                                "RegisterProduct\t\t6100\n"
                                                "PublishFeatures\t\t6300\n"
                                                "PublishProduct\t\t6400\n"
                                                "InstallFinalize\t\t6600";

static const CHAR vp_property_dat[] = "Property\tValue\n"
                                      "s72\tl0\n"
                                      "Property\tProperty\n"
                                      "HASUIRUN\t0\n"
                                      "INSTALLLEVEL\t3\n"
                                      "InstallMode\tTypical\n"
                                      "Manufacturer\tWine\n"
                                      "PIDTemplate\t###-#######\n"
                                      "ProductCode\t{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}\n"
                                      "ProductLanguage\t1033\n"
                                      "ProductName\tMSITEST\n"
                                      "ProductVersion\t1.1.1\n"
                                      "UpgradeCode\t{4C0EAA15-0264-4E5A-8758-609EF142B92D}\n";

static const CHAR vp_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                           "s72\ti2\tS64\tS0\tS255\n"
                                           "CustomAction\tAction\n"
                                           "SetProductID1\t51\tProductID\t1\t\n"
                                           "SetProductID2\t51\tProductID\t2\t\n"
                                           "TestProductID1\t19\t\t\tHalts installation\n"
                                           "TestProductID2\t19\t\t\tHalts installation\n";

static const CHAR vp_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "LaunchConditions\t\t100\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "CostFinalize\t\t1000\n"
                                              "InstallValidate\t\t1400\n"
                                              "InstallInitialize\t\t1500\n"
                                              "SetProductID1\tSET_PRODUCT_ID=1\t3000\n"
                                              "SetProductID2\tSET_PRODUCT_ID=2\t3100\n"
                                              "ValidateProductID\t\t3200\n"
                                              "InstallExecute\t\t3300\n"
                                              "TestProductID1\tProductID=1\t3400\n"
                                              "TestProductID2\tProductID=\"123-1234567\"\t3500\n"
                                              "InstallFiles\t\t4000\n"
                                              "InstallFinalize\t\t6000\n";

static const CHAR odbc_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                    "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                    "File\tFile\n"
                                    "ODBCdriver.dll\todbc\tODBCdriver.dll\t1000\t\t\t8192\t1\n"
                                    "ODBCdriver2.dll\todbc\tODBCdriver2.dll\t1000\t\t\t8192\t2\n"
                                    "ODBCtranslator.dll\todbc\tODBCtranslator.dll\t1000\t\t\t8192\t3\n"
                                    "ODBCtranslator2.dll\todbc\tODBCtranslator2.dll\t1000\t\t\t8192\t4\n"
                                    "ODBCsetup.dll\todbc\tODBCsetup.dll\t1000\t\t\t8192\t5\n";

static const CHAR odbc_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                       "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                       "Feature\tFeature\n"
                                       "odbc\t\t\todbc feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR odbc_feature_comp_dat[] = "Feature_\tComponent_\n"
                                            "s38\ts72\n"
                                            "FeatureComponents\tFeature_\tComponent_\n"
                                            "odbc\todbc\n";

static const CHAR odbc_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                         "s72\tS38\ts72\ti2\tS255\tS72\n"
                                         "Component\tComponent\n"
                                         "odbc\t{B6F3E4AE-35D1-4B72-9044-989F03E20A43}\tMSITESTDIR\t0\t\tODBCdriver.dll\n";

static const CHAR odbc_driver_dat[] = "Driver\tComponent_\tDescription\tFile_\tFile_Setup\n"
                                      "s72\ts72\ts255\ts72\tS72\n"
                                      "ODBCDriver\tDriver\n"
                                      "ODBC test driver\todbc\tODBC test driver\tODBCdriver.dll\t\n"
                                      "ODBC test driver2\todbc\tODBC test driver2\tODBCdriver2.dll\tODBCsetup.dll\n";

static const CHAR odbc_translator_dat[] = "Translator\tComponent_\tDescription\tFile_\tFile_Setup\n"
                                          "s72\ts72\ts255\ts72\tS72\n"
                                          "ODBCTranslator\tTranslator\n"
                                          "ODBC test translator\todbc\tODBC test translator\tODBCtranslator.dll\t\n"
                                          "ODBC test translator2\todbc\tODBC test translator2\tODBCtranslator2.dll\tODBCsetup.dll\n";

static const CHAR odbc_datasource_dat[] = "DataSource\tComponent_\tDescription\tDriverDescription\tRegistration\n"
                                          "s72\ts72\ts255\ts255\ti2\n"
                                          "ODBCDataSource\tDataSource\n"
                                          "ODBC data source\todbc\tODBC data source\tODBC driver\t0\n";

static const CHAR odbc_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                                "s72\tS255\tI2\n"
                                                "InstallExecuteSequence\tAction\n"
                                                "LaunchConditions\t\t100\n"
                                                "CostInitialize\t\t800\n"
                                                "FileCost\t\t900\n"
                                                "CostFinalize\t\t1000\n"
                                                "InstallValidate\t\t1400\n"
                                                "InstallInitialize\t\t1500\n"
                                                "ProcessComponents\t\t1600\n"
                                                "InstallODBC\t\t3000\n"
                                                "RemoveODBC\t\t3100\n"
                                                "RemoveFiles\t\t3900\n"
                                                "InstallFiles\t\t4000\n"
                                                "RegisterProduct\t\t5000\n"
                                                "PublishFeatures\t\t5100\n"
                                                "PublishProduct\t\t5200\n"
                                                "InstallFinalize\t\t6000\n";

static const CHAR odbc_media_dat[] = "DiskId\tLastSequence\tDiskPrompt\tCabinet\tVolumeLabel\tSource\n"
                                     "i2\ti4\tL64\tS255\tS32\tS72\n"
                                     "Media\tDiskId\n"
                                     "1\t5\t\t\tDISK1\t\n";

static const CHAR tl_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "typelib.dll\ttypelib\ttypelib.dll\t1000\t\t\t8192\t1\n";

static const CHAR tl_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                     "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                     "Feature\tFeature\n"
                                     "typelib\t\t\ttypelib feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR tl_feature_comp_dat[] = "Feature_\tComponent_\n"
                                          "s38\ts72\n"
                                          "FeatureComponents\tFeature_\tComponent_\n"
                                          "typelib\ttypelib\n";

static const CHAR tl_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "typelib\t{BB4C26FD-89D8-4E49-AF1C-DB4DCB5BF1B0}\tMSITESTDIR\t0\t\ttypelib.dll\n";

static const CHAR tl_typelib_dat[] = "LibID\tLanguage\tComponent_\tVersion\tDescription\tDirectory_\tFeature_\tCost\n"
                                     "s38\ti2\ts72\tI4\tL128\tS72\ts38\tI4\n"
                                     "TypeLib\tLibID\tLanguage\tComponent_\n"
                                     "{EAC5166A-9734-4D91-878F-1DD02304C66C}\t0\ttypelib\t1793\t\tMSITESTDIR\ttypelib\t\n";

static const CHAR tl_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "LaunchConditions\t\t100\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "CostFinalize\t\t1000\n"
                                              "InstallValidate\t\t1400\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\t\t1600\n"
                                              "RemoveFiles\t\t1700\n"
                                              "InstallFiles\t\t2000\n"
                                              "RegisterTypeLibraries\tREGISTER_TYPELIB=1\t3000\n"
                                              "UnregisterTypeLibraries\t\t3100\n"
                                              "RegisterProduct\t\t5100\n"
                                              "PublishFeatures\t\t5200\n"
                                              "PublishProduct\t\t5300\n"
                                              "InstallFinalize\t\t6000\n";

static const CHAR crs_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "target.txt\tshortcut\ttarget.txt\t1000\t\t\t8192\t1\n";

static const CHAR crs_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "shortcut\t\t\tshortcut feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR crs_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "shortcut\tshortcut\n";

static const CHAR crs_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "shortcut\t{5D20E3C6-7206-498F-AC28-87AF2F9AD4CC}\tMSITESTDIR\t0\t\ttarget.txt\n";

static const CHAR crs_shortcut_dat[] = "Shortcut\tDirectory_\tName\tComponent_\tTarget\tArguments\tDescription\tHotkey\tIcon_\tIconIndex\tShowCmd\tWkDir\n"
                                       "s72\ts72\tl128\ts72\ts72\tL255\tL255\tI2\tS72\tI2\tI2\tS72\n"
                                       "Shortcut\tShortcut\n"
                                       "shortcut\tMSITESTDIR\tshortcut\tshortcut\t[MSITESTDIR]target.txt\t\t\t\t\t\t\t\n";

static const CHAR crs_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "RemoveShortcuts\t\t3000\n"
                                               "CreateShortcuts\t\t3100\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR fo_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "override.txt\toverride\toverride.txt\t1000\t\t\t8192\t1\n"
                                  "preselected.txt\tpreselected\tpreselected.txt\t1000\t\t\t8192\t2\n"
                                  "notpreselected.txt\tnotpreselected\tnotpreselected.txt\t1000\t\t\t8192\t3\n";

static const CHAR fo_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                     "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                     "Feature\tFeature\n"
                                     "override\t\t\toverride feature\t1\t1\tMSITESTDIR\t0\n"
                                     "preselected\t\t\tpreselected feature\t1\t1\tMSITESTDIR\t0\n"
                                     "notpreselected\t\t\tnotpreselected feature\t1\t1\tMSITESTDIR\t0\n";

static const CHAR fo_condition_dat[] = "Feature_\tLevel\tCondition\n"
                                       "s38\ti2\tS255\n"
                                       "Condition\tFeature_\tLevel\n"
                                       "preselected\t0\tPreselected\n"
                                       "notpreselected\t0\tNOT Preselected\n";

static const CHAR fo_feature_comp_dat[] = "Feature_\tComponent_\n"
                                          "s38\ts72\n"
                                          "FeatureComponents\tFeature_\tComponent_\n"
                                          "override\toverride\n"
                                          "preselected\tpreselected\n"
                                          "notpreselected\tnotpreselected\n";

static const CHAR fo_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "override\t{0A00FB1D-97B0-4B42-ADF0-BB8913416623}\tMSITESTDIR\t0\t\toverride.txt\n"
                                       "preselected\t{44E1DB75-605A-43DD-8CF5-CAB17F1BBD60}\tMSITESTDIR\t0\t\tpreselected.txt\n"
                                       "notpreselected\t{E1647733-5E75-400A-A92E-5E60B4D4EF9F}\tMSITESTDIR\t0\t\tnotpreselected.txt\n";

static const CHAR fo_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                           "s72\ti2\tS64\tS0\tS255\n"
                                           "CustomAction\tAction\n"
                                           "SetPreselected\t51\tPreselected\t1\t\n";

static const CHAR fo_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "LaunchConditions\t\t100\n"
                                              "SetPreselected\tpreselect=1\t200\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "CostFinalize\t\t1000\n"
                                              "InstallValidate\t\t1400\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\t\t1600\n"
                                              "RemoveFiles\t\t1700\n"
                                              "InstallFiles\t\t2000\n"
                                              "RegisterProduct\t\t5000\n"
                                              "PublishFeatures\t\t5100\n"
                                              "PublishProduct\t\t5200\n"
                                              "InstallFinalize\t\t6000\n";

static const CHAR pub_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "english.txt\tpublish\tenglish.txt\t1000\t\t\t8192\t1\n";

static const CHAR pub_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "publish\t\t\tpublish feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR pub_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "publish\tpublish\n";

static const CHAR pub_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "publish\t{B4EA0ACF-6238-426E-9C6D-7869F0F9C768}\tMSITESTDIR\t0\t\tenglish.txt\n";

static const CHAR pub_publish_component_dat[] = "ComponentId\tQualifier\tComponent_\tAppData\tFeature_\n"
                                                "s38\ts255\ts72\tL255\ts38\n"
                                                "PublishComponent\tComponentId\tQualifier\tComponent_\n"
                                                "{92AFCBC0-9CA6-4270-8454-47C5EE2B8FAA}\tenglish.txt\tpublish\t\tpublish\n";

static const CHAR pub_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "PublishComponents\t\t3000\n"
                                               "UnpublishComponents\t\t3100\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR rd_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "original.txt\tduplicate\toriginal.txt\t1000\t\t\t8192\t1\n"
                                  "original2.txt\tduplicate\toriginal2.txt\t1000\t\t\t8192\t2\n"
                                  "original3.txt\tduplicate2\toriginal3.txt\t1000\t\t\t8192\t3\n";

static const CHAR rd_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                     "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                     "Feature\tFeature\n"
                                     "duplicate\t\t\tduplicate feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR rd_feature_comp_dat[] = "Feature_\tComponent_\n"
                                          "s38\ts72\n"
                                          "FeatureComponents\tFeature_\tComponent_\n"
                                          "duplicate\tduplicate\n";

static const CHAR rd_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "duplicate\t{EB45D06A-ADFE-44E3-8D41-B7DE150E41AD}\tMSITESTDIR\t0\t\toriginal.txt\n"
                                       "duplicate2\t{B8BA60E0-B2E9-488E-9D0E-E60F25F04F97}\tMSITESTDIR\t0\tDUPLICATE2=1\toriginal3.txt\n";

static const CHAR rd_duplicate_file_dat[] = "FileKey\tComponent_\tFile_\tDestName\tDestFolder\n"
                                            "s72\ts72\ts72\tS255\tS72\n"
                                            "DuplicateFile\tFileKey\n"
                                            "duplicate\tduplicate\toriginal.txt\tduplicate.txt\t\n"
                                            "duplicate2\tduplicate\toriginal2.txt\t\tMSITESTDIR\n"
                                            "duplicate3\tduplicate2\toriginal3.txt\tduplicate2.txt\t\n";

static const CHAR rd_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "LaunchConditions\t\t100\n"
                                              "CostInitialize\t\t800\n"
                                              "FileCost\t\t900\n"
                                              "CostFinalize\t\t1000\n"
                                              "InstallValidate\t\t1400\n"
                                              "InstallInitialize\t\t1500\n"
                                              "ProcessComponents\t\t1600\n"
                                              "RemoveDuplicateFiles\t\t1900\n"
                                              "InstallFiles\t\t2000\n"
                                              "DuplicateFiles\t\t2100\n"
                                              "RegisterProduct\t\t5000\n"
                                              "PublishFeatures\t\t5100\n"
                                              "PublishProduct\t\t5200\n"
                                              "InstallFinalize\t\t6000\n";

static const CHAR rrv_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "registry.txt\tregistry\tregistry.txt\t1000\t\t\t8192\t1\n";

static const CHAR rrv_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "registry\t\t\tregistry feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR rrv_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "registry\tregistry\n";

static const CHAR rrv_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "registry\t{DA97585B-962D-45EB-AD32-DA15E60CA9EE}\tMSITESTDIR\t0\t\tregistry.txt\n";

static const CHAR rrv_registry_dat[] = "Registry\tRoot\tKey\tName\tValue\tComponent_\n"
                                       "s72\ti2\tl255\tL255\tL0\ts72\n"
                                       "Registry\tRegistry\n"
                                       "reg1\t2\tSOFTWARE\\Wine\\keyA\t\tA\tregistry\n"
                                       "reg2\t2\tSOFTWARE\\Wine\\keyA\tvalueA\tA\tregistry\n"
                                       "reg3\t2\tSOFTWARE\\Wine\\key1\t-\t\tregistry\n";

static const CHAR rrv_remove_registry_dat[] = "RemoveRegistry\tRoot\tKey\tName\tComponent_\n"
                                              "s72\ti2\tl255\tL255\ts72\n"
                                              "RemoveRegistry\tRemoveRegistry\n"
                                              "reg1\t2\tSOFTWARE\\Wine\\keyB\t\tregistry\n"
                                              "reg2\t2\tSOFTWARE\\Wine\\keyB\tValueB\tregistry\n"
                                              "reg3\t2\tSOFTWARE\\Wine\\key2\t-\tregistry\n";

static const CHAR rrv_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "RemoveRegistryValues\t\t3000\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR frp_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "product.txt\tproduct\tproduct.txt\t1000\t\t\t8192\t1\n";

static const CHAR frp_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "product\t\t\tproduct feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR frp_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "product\tproduct\n";

static const CHAR frp_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "product\t{44725EE0-EEA8-40BD-8162-A48224A2FEA1}\tMSITESTDIR\t0\t\tproduct.txt\n";

static const CHAR frp_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                            "s72\ti2\tS64\tS0\tS255\n"
                                            "CustomAction\tAction\n"
                                            "TestProp\t19\t\t\tPROP set\n";

static const CHAR frp_upgrade_dat[] = "UpgradeCode\tVersionMin\tVersionMax\tLanguage\tAttributes\tRemove\tActionProperty\n"
                                      "s38\tS20\tS20\tS255\ti4\tS255\ts72\n"
                                      "Upgrade\tUpgradeCode\tVersionMin\tVersionMax\tLanguage\tAttributes\n"
                                      "{4C0EAA15-0264-4E5A-8758-609EF142B92D}\t1.1.1\t2.2.2\t\t768\t\tPROP\n";

static const CHAR frp_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "FindRelatedProducts\t\t50\n"
                                               "TestProp\tPROP AND NOT REMOVE\t51\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR riv_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "inifile.txt\tinifile\tinifile.txt\t1000\t\t\t8192\t1\n";

static const CHAR riv_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "inifile\t\t\tinifile feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR riv_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "inifile\tinifile\n";

static const CHAR riv_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "inifile\t{A0F15705-4F57-4437-88C4-6C8B37ACC6DE}\tMSITESTDIR\t0\t\tinifile.txt\n";

static const CHAR riv_ini_file_dat[] = "IniFile\tFileName\tDirProperty\tSection\tKey\tValue\tAction\tComponent_\n"
                                       "s72\tl255\tS72\tl96\tl128\tl255\ti2\ts72\n"
                                       "IniFile\tIniFile\n"
                                       "inifile1\ttest.ini\tMSITESTDIR\tsection1\tkey1\tvalue1\t0\tinifile\n";

static const CHAR riv_remove_ini_file_dat[] = "RemoveIniFile\tFileName\tDirProperty\tSection\tKey\tValue\tAction\tComponent_\n"
                                              "s72\tl255\tS72\tl96\tl128\tL255\ti2\ts72\n"
                                              "RemoveIniFile\tRemoveIniFile\n"
                                              "inifile1\ttest.ini\tMSITESTDIR\tsectionA\tkeyA\tvalueA\t2\tinifile\n";

static const CHAR riv_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "RemoveIniValues\t\t3000\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR res_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "envvar.txt\tenvvar\tenvvar.txt\t1000\t\t\t8192\t1\n";

static const CHAR res_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "envvar\t\t\tenvvar feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR res_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "envvar\tenvvar\n";

static const CHAR res_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "envvar\t{45EE9AF4-E5D1-445F-8BB7-B22D4EEBD29E}\tMSITESTDIR\t0\t\tenvvar.txt\n";

static const CHAR res_environment_dat[] = "Environment\tName\tValue\tComponent_\n"
                                          "s72\tl255\tL255\ts72\n"
                                          "Environment\tEnvironment\n"
                                          "var1\t=-MSITESTVAR1\t1\tenvvar\n"
                                          "var2\t=+-MSITESTVAR2\t1\tenvvar\n"
                                          "var3\t=MSITESTVAR3\t1\tenvvar\n"
                                          "var4\t=-MSITESTVAR4\t\tenvvar\n"
                                          "var5\t=MSITESTVAR5\t\tenvvar\n";

static const CHAR res_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "RemoveEnvironmentStrings\t\t3000\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR rci_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "class.txt\tclass\tclass.txt\t1000\t\t\t8192\t1\n";

static const CHAR rci_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "class\t\t\tclass feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR rci_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "class\tclass\n";

static const CHAR rci_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "class\t{89A98345-F8A1-422E-A48B-0250B5809F2D}\tMSITESTDIR\t0\t\tclass.txt\n";

static const CHAR rci_appid_dat[] = "AppId\tRemoteServerName\tLocalService\tServiceParameters\tDllSurrogate\tActivateAtStorage\tRunAsInteractiveUser\n"
                                    "s38\tS255\tS255\tS255\tS255\tI2\tI2\n"
                                    "AppId\tAppId\n"
                                    "{CFCC3B38-E683-497D-9AB4-CB40AAFE307F}\t\t\t\t\t\t\n";

static const CHAR rci_class_dat[] = "CLSID\tContext\tComponent_\tProgId_Default\tDescription\tAppId_\tFileTypeMask\tIcon_\tIconIndex\tDefInprocHandler\tArgument\tFeature_\tAttributes\n"
                                    "s38\ts32\ts72\tS255\tL255\tS38\tS255\tS72\tI2\tS32\tS255\ts38\tI2\n"
                                    "Class\tCLSID\tContext\tComponent_\n"
                                    "{110913E7-86D1-4BF3-9922-BA103FCDDDFA}\tLocalServer\tclass\t\tdescription\t{CFCC3B38-E683-497D-9AB4-CB40AAFE307F}\tmask1;mask2\t\t\t2\t\tclass\t\n";

static const CHAR rci_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "UnregisterClassInfo\t\t3000\n"
                                               "RegisterClassInfo\t\t4000\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR rei_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "extension.txt\textension\textension.txt\t1000\t\t\t8192\t1\n";

static const CHAR rei_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "extension\t\t\textension feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR rei_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "extension\textension\n";

static const CHAR rei_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "extension\t{9A3060D4-60BA-4A82-AB55-9FB148AD013C}\tMSITESTDIR\t0\t\textension.txt\n";

static const CHAR rei_extension_dat[] = "Extension\tComponent_\tProgId_\tMIME_\tFeature_\n"
                                        "s255\ts72\tS255\tS64\ts38\n"
                                        "Extension\tExtension\tComponent_\n"
                                        "extension\textension\tProg.Id.1\t\textension\n";

static const CHAR rei_verb_dat[] = "Extension_\tVerb\tSequence\tCommand\tArgument\n"
                                   "s255\ts32\tI2\tL255\tL255\n"
                                   "Verb\tExtension_\tVerb\n"
                                   "extension\tOpen\t1\t&Open\t/argument\n";

static const CHAR rei_progid_dat[] = "ProgId\tProgId_Parent\tClass_\tDescription\tIcon_\tIconIndex\n"
                                     "s255\tS255\tS38\tL255\tS72\tI2\n"
                                     "ProgId\tProgId\n"
                                     "Prog.Id.1\t\t\tdescription\t\t\n";

static const CHAR rei_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "UnregisterExtensionInfo\t\t3000\n"
                                               "RegisterExtensionInfo\t\t4000\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR rmi_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                   "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                   "File\tFile\n"
                                   "mime.txt\tmime\tmime.txt\t1000\t\t\t8192\t1\n";

static const CHAR rmi_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                      "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                      "Feature\tFeature\n"
                                      "mime\t\t\tmime feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR rmi_feature_comp_dat[] = "Feature_\tComponent_\n"
                                           "s38\ts72\n"
                                           "FeatureComponents\tFeature_\tComponent_\n"
                                           "mime\tmime\n";

static const CHAR rmi_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                        "s72\tS38\ts72\ti2\tS255\tS72\n"
                                        "Component\tComponent\n"
                                        "mime\t{A1D630CE-13A7-4882-AFDD-148E2BBAFC6D}\tMSITESTDIR\t0\t\tmime.txt\n";

static const CHAR rmi_extension_dat[] = "Extension\tComponent_\tProgId_\tMIME_\tFeature_\n"
                                        "s255\ts72\tS255\tS64\ts38\n"
                                        "Extension\tExtension\tComponent_\n"
                                        "mime\tmime\t\tmime/type\tmime\n";

static const CHAR rmi_verb_dat[] = "Extension_\tVerb\tSequence\tCommand\tArgument\n"
                                   "s255\ts32\tI2\tL255\tL255\n"
                                   "Verb\tExtension_\tVerb\n"
                                   "mime\tOpen\t1\t&Open\t/argument\n";

static const CHAR rmi_mime_dat[] = "ContentType\tExtension_\tCLSID\n"
                                   "s64\ts255\tS38\n"
                                   "MIME\tContentType\n"
                                   "mime/type\tmime\t\n";

static const CHAR rmi_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                               "s72\tS255\tI2\n"
                                               "InstallExecuteSequence\tAction\n"
                                               "LaunchConditions\t\t100\n"
                                               "CostInitialize\t\t800\n"
                                               "FileCost\t\t900\n"
                                               "CostFinalize\t\t1000\n"
                                               "InstallValidate\t\t1400\n"
                                               "InstallInitialize\t\t1500\n"
                                               "ProcessComponents\t\t1600\n"
                                               "RemoveFiles\t\t1700\n"
                                               "InstallFiles\t\t2000\n"
                                               "UnregisterExtensionInfo\t\t3000\n"
                                               "UnregisterMIMEInfo\t\t3500\n"
                                               "RegisterExtensionInfo\t\t4000\n"
                                               "RegisterMIMEInfo\t\t4500\n"
                                               "RegisterProduct\t\t5000\n"
                                               "PublishFeatures\t\t5100\n"
                                               "PublishProduct\t\t5200\n"
                                               "InstallFinalize\t\t6000\n";

static const CHAR sd_file_dat[] = "File\tComponent_\tFileName\tFileSize\tVersion\tLanguage\tAttributes\tSequence\n"
                                  "s72\ts72\tl255\ti4\tS72\tS20\tI2\ti2\n"
                                  "File\tFile\n"
                                  "sourcedir.txt\tsourcedir\tsourcedir.txt\t1000\t\t\t8192\t1\n";

static const CHAR sd_feature_dat[] = "Feature\tFeature_Parent\tTitle\tDescription\tDisplay\tLevel\tDirectory_\tAttributes\n"
                                     "s38\tS38\tL64\tL255\tI2\ti2\tS72\ti2\n"
                                     "Feature\tFeature\n"
                                     "sourcedir\t\t\tsourcedir feature\t1\t2\tMSITESTDIR\t0\n";

static const CHAR sd_feature_comp_dat[] = "Feature_\tComponent_\n"
                                          "s38\ts72\n"
                                          "FeatureComponents\tFeature_\tComponent_\n"
                                          "sourcedir\tsourcedir\n";

static const CHAR sd_component_dat[] = "Component\tComponentId\tDirectory_\tAttributes\tCondition\tKeyPath\n"
                                       "s72\tS38\ts72\ti2\tS255\tS72\n"
                                       "Component\tComponent\n"
                                       "sourcedir\t{DD422F92-3ED8-49B5-A0B7-F266F98357DF}\tMSITESTDIR\t0\t\tsourcedir.txt\n";

static const CHAR sd_install_ui_seq_dat[] = "Action\tCondition\tSequence\n"
                                            "s72\tS255\tI2\n"
                                            "InstallUISequence\tAction\n"
                                            "TestSourceDirProp1\tnot SourceDir and not SOURCEDIR and not Installed\t99\n"
                                            "AppSearch\t\t100\n"
                                            "TestSourceDirProp2\tnot SourceDir and not SOURCEDIR and not Installed\t101\n"
                                            "LaunchConditions\tnot Installed \t110\n"
                                            "TestSourceDirProp3\tnot SourceDir and not SOURCEDIR and not Installed\t111\n"
                                            "FindRelatedProducts\t\t120\n"
                                            "TestSourceDirProp4\tnot SourceDir and not SOURCEDIR and not Installed\t121\n"
                                            "CCPSearch\t\t130\n"
                                            "TestSourceDirProp5\tnot SourceDir and not SOURCEDIR and not Installed\t131\n"
                                            "RMCCPSearch\t\t140\n"
                                            "TestSourceDirProp6\tnot SourceDir and not SOURCEDIR and not Installed\t141\n"
                                            "ValidateProductID\t\t150\n"
                                            "TestSourceDirProp7\tnot SourceDir and not SOURCEDIR and not Installed\t151\n"
                                            "CostInitialize\t\t800\n"
                                            "TestSourceDirProp8\tnot SourceDir and not SOURCEDIR and not Installed\t801\n"
                                            "FileCost\t\t900\n"
                                            "TestSourceDirProp9\tnot SourceDir and not SOURCEDIR and not Installed\t901\n"
                                            "IsolateComponents\t\t1000\n"
                                            "TestSourceDirProp10\tnot SourceDir and not SOURCEDIR and not Installed\t1001\n"
                                            "CostFinalize\t\t1100\n"
                                            "TestSourceDirProp11\tnot SourceDir and not SOURCEDIR and not Installed\t1101\n"
                                            "MigrateFeatureStates\t\t1200\n"
                                            "TestSourceDirProp12\tnot SourceDir and not SOURCEDIR and not Installed\t1201\n"
                                            "ExecuteAction\t\t1300\n"
                                            "TestSourceDirProp13\tnot SourceDir and not SOURCEDIR and not Installed\t1301\n";

static const CHAR sd_install_exec_seq_dat[] = "Action\tCondition\tSequence\n"
                                              "s72\tS255\tI2\n"
                                              "InstallExecuteSequence\tAction\n"
                                              "TestSourceDirProp14\tSourceDir and SOURCEDIR and not Installed\t99\n"
                                              "LaunchConditions\t\t100\n"
                                              "TestSourceDirProp15\tSourceDir and SOURCEDIR and not Installed\t101\n"
                                              "ValidateProductID\t\t700\n"
                                              "TestSourceDirProp16\tSourceDir and SOURCEDIR and not Installed\t701\n"
                                              "CostInitialize\t\t800\n"
                                              "TestSourceDirProp17\tSourceDir and SOURCEDIR and not Installed\t801\n"
                                              "ResolveSource\tResolveSource and not Installed\t850\n"
                                              "TestSourceDirProp18\tResolveSource and not SourceDir and not SOURCEDIR and not Installed\t851\n"
                                              "TestSourceDirProp19\tnot ResolveSource and SourceDir and SOURCEDIR and not Installed\t852\n"
                                              "FileCost\t\t900\n"
                                              "TestSourceDirProp20\tSourceDir and SOURCEDIR and not Installed\t901\n"
                                              "IsolateComponents\t\t1000\n"
                                              "TestSourceDirProp21\tSourceDir and SOURCEDIR and not Installed\t1001\n"
                                              "CostFinalize\t\t1100\n"
                                              "TestSourceDirProp22\tSourceDir and SOURCEDIR and not Installed\t1101\n"
                                              "MigrateFeatureStates\t\t1200\n"
                                              "TestSourceDirProp23\tSourceDir and SOURCEDIR and not Installed\t1201\n"
                                              "InstallValidate\t\t1400\n"
                                              "TestSourceDirProp24\tSourceDir and SOURCEDIR and not Installed\t1401\n"
                                              "InstallInitialize\t\t1500\n"
                                              "TestSourceDirProp25\tSourceDir and SOURCEDIR and not Installed\t1501\n"
                                              "ProcessComponents\t\t1600\n"
                                              "TestSourceDirProp26\tnot SourceDir and not SOURCEDIR and not Installed\t1601\n"
                                              "UnpublishFeatures\t\t1800\n"
                                              "TestSourceDirProp27\tnot SourceDir and not SOURCEDIR and not Installed\t1801\n"
                                              "RemoveFiles\t\t3500\n"
                                              "TestSourceDirProp28\tnot SourceDir and not SOURCEDIR and not Installed\t3501\n"
                                              "InstallFiles\t\t4000\n"
                                              "TestSourceDirProp29\tnot SourceDir and not SOURCEDIR and not Installed\t4001\n"
                                              "RegisterUser\t\t6000\n"
                                              "TestSourceDirProp30\tnot SourceDir and not SOURCEDIR and not Installed\t6001\n"
                                              "RegisterProduct\t\t6100\n"
                                              "TestSourceDirProp31\tnot SourceDir and not SOURCEDIR and not Installed\t6101\n"
                                              "PublishFeatures\t\t6300\n"
                                              "TestSourceDirProp32\tnot SourceDir and not SOURCEDIR and not Installed\t6301\n"
                                              "PublishProduct\t\t6400\n"
                                              "TestSourceDirProp33\tnot SourceDir and not SOURCEDIR and not Installed\t6401\n"
                                              "InstallExecute\t\t6500\n"
                                              "TestSourceDirProp34\tnot SourceDir and not SOURCEDIR and not Installed\t6501\n"
                                              "InstallFinalize\t\t6600\n"
                                              "TestSourceDirProp35\tnot SourceDir and not SOURCEDIR and not Installed\t6601\n";

static const CHAR sd_custom_action_dat[] = "Action\tType\tSource\tTarget\tISComments\n"
                                           "s72\ti2\tS64\tS0\tS255\n"
                                           "CustomAction\tAction\n"
                                           "TestSourceDirProp1\t19\t\tTest 1 failed\t\n"
                                           "TestSourceDirProp2\t19\t\tTest 2 failed\t\n"
                                           "TestSourceDirProp3\t19\t\tTest 3 failed\t\n"
                                           "TestSourceDirProp4\t19\t\tTest 4 failed\t\n"
                                           "TestSourceDirProp5\t19\t\tTest 5 failed\t\n"
                                           "TestSourceDirProp6\t19\t\tTest 6 failed\t\n"
                                           "TestSourceDirProp7\t19\t\tTest 7 failed\t\n"
                                           "TestSourceDirProp8\t19\t\tTest 8 failed\t\n"
                                           "TestSourceDirProp9\t19\t\tTest 9 failed\t\n"
                                           "TestSourceDirProp10\t19\t\tTest 10 failed\t\n"
                                           "TestSourceDirProp11\t19\t\tTest 11 failed\t\n"
                                           "TestSourceDirProp12\t19\t\tTest 12 failed\t\n"
                                           "TestSourceDirProp13\t19\t\tTest 13 failed\t\n"
                                           "TestSourceDirProp14\t19\t\tTest 14 failed\t\n"
                                           "TestSourceDirProp15\t19\t\tTest 15 failed\t\n"
                                           "TestSourceDirProp16\t19\t\tTest 16 failed\t\n"
                                           "TestSourceDirProp17\t19\t\tTest 17 failed\t\n"
                                           "TestSourceDirProp18\t19\t\tTest 18 failed\t\n"
                                           "TestSourceDirProp19\t19\t\tTest 19 failed\t\n"
                                           "TestSourceDirProp20\t19\t\tTest 20 failed\t\n"
                                           "TestSourceDirProp21\t19\t\tTest 21 failed\t\n"
                                           "TestSourceDirProp22\t19\t\tTest 22 failed\t\n"
                                           "TestSourceDirProp23\t19\t\tTest 23 failed\t\n"
                                           "TestSourceDirProp24\t19\t\tTest 24 failed\t\n"
                                           "TestSourceDirProp25\t19\t\tTest 25 failed\t\n"
                                           "TestSourceDirProp26\t19\t\tTest 26 failed\t\n"
                                           "TestSourceDirProp27\t19\t\tTest 27 failed\t\n"
                                           "TestSourceDirProp28\t19\t\tTest 28 failed\t\n"
                                           "TestSourceDirProp29\t19\t\tTest 29 failed\t\n"
                                           "TestSourceDirProp30\t19\t\tTest 30 failed\t\n"
                                           "TestSourceDirProp31\t19\t\tTest 31 failed\t\n"
                                           "TestSourceDirProp32\t19\t\tTest 32 failed\t\n"
                                           "TestSourceDirProp33\t19\t\tTest 33 failed\t\n"
                                           "TestSourceDirProp34\t19\t\tTest 34 failed\t\n"
                                           "TestSourceDirProp35\t19\t\tTest 35 failed\t\n";

typedef struct _msi_table
{
    const CHAR *filename;
    const CHAR *data;
    int size;
} msi_table;

#define ADD_TABLE(x) {#x".idt", x##_dat, sizeof(x##_dat)}

static const msi_table tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table sc_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property),
    ADD_TABLE(shortcut)
};

static const msi_table ps_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property),
    ADD_TABLE(condition)
};

static const msi_table env_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property),
    ADD_TABLE(environment)
};

static const msi_table up_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(up_property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table up2_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(up2_property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table up3_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(up3_property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table up4_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table up5_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(up_property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table up6_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(up2_property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table up7_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(up3_property),
    ADD_TABLE(registry),
    ADD_TABLE(service_install),
    ADD_TABLE(service_control)
};

static const msi_table cc_tables[] =
{
    ADD_TABLE(cc_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(cc_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(cc_media),
    ADD_TABLE(property),
};

static const msi_table cc2_tables[] =
{
    ADD_TABLE(cc2_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(cc2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(cc_media),
    ADD_TABLE(property),
};

static const msi_table co_tables[] =
{
    ADD_TABLE(cc_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(co_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(co_media),
    ADD_TABLE(property),
};

static const msi_table co2_tables[] =
{
    ADD_TABLE(cc_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(cc_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(co2_media),
    ADD_TABLE(property),
};

static const msi_table mm_tables[] =
{
    ADD_TABLE(cc_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(mm_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(mm_media),
    ADD_TABLE(property),
};

static const msi_table ss_tables[] =
{
    ADD_TABLE(cc_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(cc_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(ss_media),
    ADD_TABLE(property),
};

static const msi_table ui_tables[] =
{
    ADD_TABLE(ui_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cc_feature_comp),
    ADD_TABLE(cc_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(ui_install_ui_seq),
    ADD_TABLE(ui_custom_action),
    ADD_TABLE(cc_media),
    ADD_TABLE(property),
};

static const msi_table rof_tables[] =
{
    ADD_TABLE(rof_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table rofc_tables[] =
{
    ADD_TABLE(rof_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rofc_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rofc_media),
    ADD_TABLE(property),
};

static const msi_table sdp_tables[] =
{
    ADD_TABLE(rof_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(sdp_install_exec_seq),
    ADD_TABLE(sdp_custom_action),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table cie_tables[] =
{
    ADD_TABLE(cie_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cie_feature_comp),
    ADD_TABLE(cie_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(cie_media),
    ADD_TABLE(property),
};

static const msi_table ci_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(ci_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(ci_custom_action),
};

static const msi_table ci2_tables[] =
{
    ADD_TABLE(ci2_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table spf_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(spf_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(spf_custom_action),
    ADD_TABLE(spf_install_ui_seq),
};

static const msi_table pp_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table ppc_tables[] =
{
    ADD_TABLE(ppc_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ppc_feature_comp),
    ADD_TABLE(ppc_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(ppc_media),
    ADD_TABLE(property),
};

static const msi_table lus0_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table lus1_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(rofc_media),
    ADD_TABLE(property),
};

static const msi_table lus2_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(lus2_media),
    ADD_TABLE(property),
};

static const msi_table tp_tables[] =
{
    ADD_TABLE(tp_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table cwd_tables[] =
{
    ADD_TABLE(cwd_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table adm_tables[] =
{
    ADD_TABLE(adm_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(adm_custom_action),
    ADD_TABLE(adm_admin_exec_seq),
};

static const msi_table amp_tables[] =
{
    ADD_TABLE(amp_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table rem_tables[] =
{
    ADD_TABLE(rem_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rem_feature_comp),
    ADD_TABLE(rem_file),
    ADD_TABLE(rem_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(rem_remove_files),
};

static const msi_table mov_tables[] =
{
    ADD_TABLE(cwd_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(mov_move_file),
};

static const msi_table mc_tables[] =
{
    ADD_TABLE(mc_component),
    ADD_TABLE(directory),
    ADD_TABLE(cc_feature),
    ADD_TABLE(cie_feature_comp),
    ADD_TABLE(mc_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(mc_media),
    ADD_TABLE(property),
    ADD_TABLE(mc_file_hash),
};

static const msi_table df_tables[] =
{
    ADD_TABLE(rof_component),
    ADD_TABLE(df_directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(df_duplicate_file),
};

static const msi_table wrv_tables[] =
{
    ADD_TABLE(wrv_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(wrv_registry),
};

static const msi_table sf_tables[] =
{
    ADD_TABLE(wrv_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table ca51_tables[] =
{
    ADD_TABLE(ca51_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(ca51_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
    ADD_TABLE(ca51_custom_action),
};

static const msi_table is_tables[] =
{
    ADD_TABLE(is_component),
    ADD_TABLE(directory),
    ADD_TABLE(is_feature),
    ADD_TABLE(is_feature_comp),
    ADD_TABLE(is_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(is_media),
    ADD_TABLE(property),
};

static const msi_table sp_tables[] =
{
    ADD_TABLE(sp_component),
    ADD_TABLE(sp_directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table mcp_tables[] =
{
    ADD_TABLE(mcp_component),
    ADD_TABLE(directory),
    ADD_TABLE(mcp_feature),
    ADD_TABLE(mcp_feature_comp),
    ADD_TABLE(rem_file),
    ADD_TABLE(rem_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table mcomp_tables[] =
{
    ADD_TABLE(mcp_component),
    ADD_TABLE(directory),
    ADD_TABLE(mcp_feature),
    ADD_TABLE(mcp_feature_comp),
    ADD_TABLE(mcomp_file),
    ADD_TABLE(rem_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table ai_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(ai_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table pc_tables[] =
{
    ADD_TABLE(ca51_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(ci2_feature_comp),
    ADD_TABLE(ci2_file),
    ADD_TABLE(install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property)
};

static const msi_table ip_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(ip_install_exec_seq),
    ADD_TABLE(ip_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table aup_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(aup_install_exec_seq),
    ADD_TABLE(aup_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table aup2_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(aup2_install_exec_seq),
    ADD_TABLE(aup_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(aup_property)
};

static const msi_table aup3_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(aup2_install_exec_seq),
    ADD_TABLE(aup_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(aup2_property)
};

static const msi_table aup4_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(aup3_install_exec_seq),
    ADD_TABLE(aup_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(aup2_property)
};

static const msi_table fiu_tables[] =
{
    ADD_TABLE(rof_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(property),
};

static const msi_table fiuc_tables[] =
{
    ADD_TABLE(rof_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rofc_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(rofc_media),
    ADD_TABLE(property),
};

static const msi_table cf_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(cf_create_folders),
    ADD_TABLE(cf_install_exec_seq),
    ADD_TABLE(cf_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table rf_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(cf_create_folders),
    ADD_TABLE(rf_install_exec_seq),
    ADD_TABLE(cf_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table sss_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(sss_install_exec_seq),
    ADD_TABLE(sss_service_control),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table sds_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(sds_install_exec_seq),
    ADD_TABLE(service_control),
    ADD_TABLE(service_install),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table sr_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(sr_selfreg),
    ADD_TABLE(sr_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table font_tables[] =
{
    ADD_TABLE(font_component),
    ADD_TABLE(directory),
    ADD_TABLE(font_feature),
    ADD_TABLE(font_feature_comp),
    ADD_TABLE(font_file),
    ADD_TABLE(font),
    ADD_TABLE(font_install_exec_seq),
    ADD_TABLE(font_media),
    ADD_TABLE(property)
};

static const msi_table vp_tables[] =
{
    ADD_TABLE(component),
    ADD_TABLE(directory),
    ADD_TABLE(feature),
    ADD_TABLE(feature_comp),
    ADD_TABLE(file),
    ADD_TABLE(vp_custom_action),
    ADD_TABLE(vp_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(vp_property)
};

static const msi_table odbc_tables[] =
{
    ADD_TABLE(odbc_component),
    ADD_TABLE(directory),
    ADD_TABLE(odbc_feature),
    ADD_TABLE(odbc_feature_comp),
    ADD_TABLE(odbc_file),
    ADD_TABLE(odbc_driver),
    ADD_TABLE(odbc_translator),
    ADD_TABLE(odbc_datasource),
    ADD_TABLE(odbc_install_exec_seq),
    ADD_TABLE(odbc_media),
    ADD_TABLE(property)
};

static const msi_table tl_tables[] =
{
    ADD_TABLE(tl_component),
    ADD_TABLE(directory),
    ADD_TABLE(tl_feature),
    ADD_TABLE(tl_feature_comp),
    ADD_TABLE(tl_file),
    ADD_TABLE(tl_typelib),
    ADD_TABLE(tl_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table crs_tables[] =
{
    ADD_TABLE(crs_component),
    ADD_TABLE(directory),
    ADD_TABLE(crs_feature),
    ADD_TABLE(crs_feature_comp),
    ADD_TABLE(crs_file),
    ADD_TABLE(crs_shortcut),
    ADD_TABLE(crs_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table pub_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(pub_component),
    ADD_TABLE(pub_feature),
    ADD_TABLE(pub_feature_comp),
    ADD_TABLE(pub_file),
    ADD_TABLE(pub_publish_component),
    ADD_TABLE(pub_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table rd_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(rd_component),
    ADD_TABLE(rd_feature),
    ADD_TABLE(rd_feature_comp),
    ADD_TABLE(rd_file),
    ADD_TABLE(rd_duplicate_file),
    ADD_TABLE(rd_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table rrv_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(rrv_component),
    ADD_TABLE(rrv_feature),
    ADD_TABLE(rrv_feature_comp),
    ADD_TABLE(rrv_file),
    ADD_TABLE(rrv_registry),
    ADD_TABLE(rrv_remove_registry),
    ADD_TABLE(rrv_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table frp_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(frp_component),
    ADD_TABLE(frp_feature),
    ADD_TABLE(frp_feature_comp),
    ADD_TABLE(frp_file),
    ADD_TABLE(frp_upgrade),
    ADD_TABLE(frp_custom_action),
    ADD_TABLE(frp_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table riv_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(riv_component),
    ADD_TABLE(riv_feature),
    ADD_TABLE(riv_feature_comp),
    ADD_TABLE(riv_file),
    ADD_TABLE(riv_ini_file),
    ADD_TABLE(riv_remove_ini_file),
    ADD_TABLE(riv_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table res_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(res_component),
    ADD_TABLE(res_feature),
    ADD_TABLE(res_feature_comp),
    ADD_TABLE(res_file),
    ADD_TABLE(res_environment),
    ADD_TABLE(res_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table rci_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(rci_component),
    ADD_TABLE(rci_feature),
    ADD_TABLE(rci_feature_comp),
    ADD_TABLE(rci_file),
    ADD_TABLE(rci_appid),
    ADD_TABLE(rci_class),
    ADD_TABLE(rci_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table rei_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(rei_component),
    ADD_TABLE(rei_feature),
    ADD_TABLE(rei_feature_comp),
    ADD_TABLE(rei_file),
    ADD_TABLE(rei_extension),
    ADD_TABLE(rei_verb),
    ADD_TABLE(rei_progid),
    ADD_TABLE(rei_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table rmi_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(rmi_component),
    ADD_TABLE(rmi_feature),
    ADD_TABLE(rmi_feature_comp),
    ADD_TABLE(rmi_file),
    ADD_TABLE(rmi_extension),
    ADD_TABLE(rmi_verb),
    ADD_TABLE(rmi_mime),
    ADD_TABLE(rmi_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table sd_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(sd_component),
    ADD_TABLE(sd_feature),
    ADD_TABLE(sd_feature_comp),
    ADD_TABLE(sd_file),
    ADD_TABLE(sd_install_exec_seq),
    ADD_TABLE(sd_install_ui_seq),
    ADD_TABLE(sd_custom_action),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table fo_tables[] =
{
    ADD_TABLE(directory),
    ADD_TABLE(fo_file),
    ADD_TABLE(fo_component),
    ADD_TABLE(fo_feature),
    ADD_TABLE(fo_condition),
    ADD_TABLE(fo_feature_comp),
    ADD_TABLE(fo_custom_action),
    ADD_TABLE(fo_install_exec_seq),
    ADD_TABLE(media),
    ADD_TABLE(property)
};

static const msi_table icon_base_tables[] =
{
    ADD_TABLE(ci_component),
    ADD_TABLE(directory),
    ADD_TABLE(rof_feature),
    ADD_TABLE(rof_feature_comp),
    ADD_TABLE(rof_file),
    ADD_TABLE(pp_install_exec_seq),
    ADD_TABLE(rof_media),
    ADD_TABLE(icon_property),
};

/* cabinet definitions */

/* make the max size large so there is only one cab file */
#define MEDIA_SIZE          0x7FFFFFFF
#define FOLDER_THRESHOLD    900000

/* the FCI callbacks */

static void * CDECL mem_alloc(ULONG cb)
{
    return HeapAlloc(GetProcessHeap(), 0, cb);
}

static void CDECL mem_free(void *memory)
{
    HeapFree(GetProcessHeap(), 0, memory);
}

static BOOL CDECL get_next_cabinet(PCCAB pccab, ULONG  cbPrevCab, void *pv)
{
    sprintf(pccab->szCab, pv, pccab->iCab);
    return TRUE;
}

static LONG CDECL progress(UINT typeStatus, ULONG cb1, ULONG cb2, void *pv)
{
    return 0;
}

static int CDECL file_placed(PCCAB pccab, char *pszFile, LONG cbFile,
                             BOOL fContinuation, void *pv)
{
    return 0;
}

static INT_PTR CDECL fci_open(char *pszFile, int oflag, int pmode, int *err, void *pv)
{
    HANDLE handle;
    DWORD dwAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreateDisposition = OPEN_EXISTING;
    
    dwAccess = GENERIC_READ | GENERIC_WRITE;
    /* FILE_SHARE_DELETE is not supported by Windows Me/98/95 */
    dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    if (GetFileAttributesA(pszFile) != INVALID_FILE_ATTRIBUTES)
        dwCreateDisposition = OPEN_EXISTING;
    else
        dwCreateDisposition = CREATE_NEW;

    handle = CreateFileA(pszFile, dwAccess, dwShareMode, NULL,
                         dwCreateDisposition, 0, NULL);

    ok(handle != INVALID_HANDLE_VALUE, "Failed to CreateFile %s\n", pszFile);

    return (INT_PTR)handle;
}

static UINT CDECL fci_read(INT_PTR hf, void *memory, UINT cb, int *err, void *pv)
{
    HANDLE handle = (HANDLE)hf;
    DWORD dwRead;
    BOOL res;
    
    res = ReadFile(handle, memory, cb, &dwRead, NULL);
    ok(res, "Failed to ReadFile\n");

    return dwRead;
}

static UINT CDECL fci_write(INT_PTR hf, void *memory, UINT cb, int *err, void *pv)
{
    HANDLE handle = (HANDLE)hf;
    DWORD dwWritten;
    BOOL res;

    res = WriteFile(handle, memory, cb, &dwWritten, NULL);
    ok(res, "Failed to WriteFile\n");

    return dwWritten;
}

static int CDECL fci_close(INT_PTR hf, int *err, void *pv)
{
    HANDLE handle = (HANDLE)hf;
    ok(CloseHandle(handle), "Failed to CloseHandle\n");

    return 0;
}

static LONG CDECL fci_seek(INT_PTR hf, LONG dist, int seektype, int *err, void *pv)
{
    HANDLE handle = (HANDLE)hf;
    DWORD ret;
    
    ret = SetFilePointer(handle, dist, NULL, seektype);
    ok(ret != INVALID_SET_FILE_POINTER, "Failed to SetFilePointer\n");

    return ret;
}

static int CDECL fci_delete(char *pszFile, int *err, void *pv)
{
    BOOL ret = DeleteFileA(pszFile);
    ok(ret, "Failed to DeleteFile %s\n", pszFile);

    return 0;
}

static void init_functionpointers(void)
{
    HMODULE hmsi = GetModuleHandleA("msi.dll");
    HMODULE hadvapi32 = GetModuleHandleA("advapi32.dll");
    HMODULE hkernel32 = GetModuleHandleA("kernel32.dll");

#define GET_PROC(mod, func) \
    p ## func = (void*)GetProcAddress(mod, #func); \
    if(!p ## func) \
      trace("GetProcAddress(%s) failed\n", #func);

    GET_PROC(hmsi, MsiQueryComponentStateA);
    GET_PROC(hmsi, MsiSetExternalUIRecord);
    GET_PROC(hmsi, MsiSourceListEnumSourcesA);
    GET_PROC(hmsi, MsiSourceListGetInfoA);

    GET_PROC(hadvapi32, ConvertSidToStringSidA);
    GET_PROC(hadvapi32, GetTokenInformation);
    GET_PROC(hadvapi32, OpenProcessToken);
    GET_PROC(hadvapi32, RegDeleteKeyExA)
    GET_PROC(hkernel32, IsWow64Process)

    hsrclient = LoadLibraryA("srclient.dll");
    GET_PROC(hsrclient, SRRemoveRestorePoint);
    GET_PROC(hsrclient, SRSetRestorePointA);

#undef GET_PROC
}

static BOOL is_process_limited(void)
{
    HANDLE token;

    if (!pOpenProcessToken || !pGetTokenInformation) return FALSE;

    if (pOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        BOOL ret;
        TOKEN_ELEVATION_TYPE type = TokenElevationTypeDefault;
        DWORD size;

        ret = pGetTokenInformation(token, TokenElevationType, &type, sizeof(type), &size);
        CloseHandle(token);
        return (ret && type == TokenElevationTypeLimited);
    }
    return FALSE;
}

static BOOL check_win9x(void)
{
    SC_HANDLE scm;

    scm = OpenSCManager(NULL, NULL, GENERIC_ALL);
    if (!scm && (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED))
        return TRUE;

    CloseServiceHandle(scm);

    return FALSE;
}

static LPSTR get_user_sid(LPSTR *usersid)
{
    HANDLE token;
    BYTE buf[1024];
    DWORD size;
    PTOKEN_USER user;

    if (!pConvertSidToStringSidA)
    {
        win_skip("ConvertSidToStringSidA is not available\n");
        return NULL;
    }

    *usersid = NULL;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    size = sizeof(buf);
    GetTokenInformation(token, TokenUser, buf, size, &size);
    user = (PTOKEN_USER)buf;
    pConvertSidToStringSidA(user->User.Sid, usersid);
    ok(*usersid != NULL, "pConvertSidToStringSidA failed lre=%d\n", GetLastError());
    CloseHandle(token);
    return *usersid;
}

static BOOL check_record(MSIHANDLE rec, UINT field, LPCSTR val)
{
    CHAR buffer[0x20];
    UINT r;
    DWORD sz;

    sz = sizeof buffer;
    r = MsiRecordGetString(rec, field, buffer, &sz);
    return (r == ERROR_SUCCESS ) && !strcmp(val, buffer);
}

static BOOL CDECL get_temp_file(char *pszTempName, int cbTempName, void *pv)
{
    LPSTR tempname;

    tempname = HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
    GetTempFileNameA(".", "xx", 0, tempname);

    if (tempname && (strlen(tempname) < (unsigned)cbTempName))
    {
        lstrcpyA(pszTempName, tempname);
        HeapFree(GetProcessHeap(), 0, tempname);
        return TRUE;
    }

    HeapFree(GetProcessHeap(), 0, tempname);

    return FALSE;
}

static INT_PTR CDECL get_open_info(char *pszName, USHORT *pdate, USHORT *ptime,
                                   USHORT *pattribs, int *err, void *pv)
{
    BY_HANDLE_FILE_INFORMATION finfo;
    FILETIME filetime;
    HANDLE handle;
    DWORD attrs;
    BOOL res;

    handle = CreateFile(pszName, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    ok(handle != INVALID_HANDLE_VALUE, "Failed to CreateFile %s\n", pszName);

    res = GetFileInformationByHandle(handle, &finfo);
    ok(res, "Expected GetFileInformationByHandle to succeed\n");
   
    FileTimeToLocalFileTime(&finfo.ftLastWriteTime, &filetime);
    FileTimeToDosDateTime(&filetime, pdate, ptime);

    attrs = GetFileAttributes(pszName);
    ok(attrs != INVALID_FILE_ATTRIBUTES, "Failed to GetFileAttributes\n");

    return (INT_PTR)handle;
}

static BOOL add_file(HFCI hfci, const char *file, TCOMP compress)
{
    char path[MAX_PATH];
    char filename[MAX_PATH];

    lstrcpyA(path, CURR_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, file);

    lstrcpyA(filename, file);

    return FCIAddFile(hfci, path, filename, FALSE, get_next_cabinet,
                      progress, get_open_info, compress);
}

static void set_cab_parameters(PCCAB pCabParams, const CHAR *name, DWORD max_size)
{
    ZeroMemory(pCabParams, sizeof(CCAB));

    pCabParams->cb = max_size;
    pCabParams->cbFolderThresh = FOLDER_THRESHOLD;
    pCabParams->setID = 0xbeef;
    pCabParams->iCab = 1;
    lstrcpyA(pCabParams->szCabPath, CURR_DIR);
    lstrcatA(pCabParams->szCabPath, "\\");
    lstrcpyA(pCabParams->szCab, name);
}

static void create_cab_file(const CHAR *name, DWORD max_size, const CHAR *files)
{
    CCAB cabParams;
    LPCSTR ptr;
    HFCI hfci;
    ERF erf;
    BOOL res;

    set_cab_parameters(&cabParams, name, max_size);

    hfci = FCICreate(&erf, file_placed, mem_alloc, mem_free, fci_open,
                      fci_read, fci_write, fci_close, fci_seek, fci_delete,
                      get_temp_file, &cabParams, NULL);

    ok(hfci != NULL, "Failed to create an FCI context\n");

    ptr = files;
    while (*ptr)
    {
        res = add_file(hfci, ptr, tcompTYPE_MSZIP);
        ok(res, "Failed to add file: %s\n", ptr);
        ptr += lstrlen(ptr) + 1;
    }

    res = FCIFlushCabinet(hfci, FALSE, get_next_cabinet, progress);
    ok(res, "Failed to flush the cabinet\n");

    res = FCIDestroy(hfci);
    ok(res, "Failed to destroy the cabinet\n");
}

static BOOL get_user_dirs(void)
{
    HKEY hkey;
    DWORD type, size;

    if(RegOpenKey(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
                   &hkey))
        return FALSE;

    size = MAX_PATH;
    if(RegQueryValueExA(hkey, "AppData", 0, &type, (LPBYTE)APP_DATA_DIR, &size)){
        RegCloseKey(hkey);
        return FALSE;
    }

    RegCloseKey(hkey);
    return TRUE;
}

static BOOL get_system_dirs(void)
{
    HKEY hkey;
    DWORD type, size;

    if (RegOpenKey(HKEY_LOCAL_MACHINE,
                   "Software\\Microsoft\\Windows\\CurrentVersion", &hkey))
        return FALSE;

    size = MAX_PATH;
    if (RegQueryValueExA(hkey, "ProgramFilesDir (x86)", 0, &type, (LPBYTE)PROG_FILES_DIR, &size) &&
        RegQueryValueExA(hkey, "ProgramFilesDir", 0, &type, (LPBYTE)PROG_FILES_DIR, &size)) {
        RegCloseKey(hkey);
        return FALSE;
    }

    size = MAX_PATH;
    if (RegQueryValueExA(hkey, "CommonFilesDir", 0, &type, (LPBYTE)COMMON_FILES_DIR, &size)) {
        RegCloseKey(hkey);
        return FALSE;
    }

    RegCloseKey(hkey);

    if(GetWindowsDirectoryA(WINDOWS_DIR, MAX_PATH) != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
}

static void create_file_data(LPCSTR name, LPCSTR data, DWORD size)
{
    HANDLE file;
    DWORD written;

    file = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;

    WriteFile(file, data, strlen(data), &written, NULL);

    if (size)
    {
        SetFilePointer(file, size, NULL, FILE_BEGIN);
        SetEndOfFile(file);
    }

    CloseHandle(file);
}

#define create_file(name, size) create_file_data(name, name, size)

static void create_test_files(void)
{
    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\one.txt", 100);
    CreateDirectoryA("msitest\\first", NULL);
    create_file("msitest\\first\\two.txt", 100);
    CreateDirectoryA("msitest\\second", NULL);
    create_file("msitest\\second\\three.txt", 100);

    create_file("four.txt", 100);
    create_file("five.txt", 100);
    create_cab_file("msitest.cab", MEDIA_SIZE, "four.txt\0five.txt\0");

    create_file("msitest\\filename", 100);
    create_file("msitest\\service.exe", 100);

    DeleteFileA("four.txt");
    DeleteFileA("five.txt");
}

static BOOL delete_pf(const CHAR *rel_path, BOOL is_file)
{
    CHAR path[MAX_PATH];

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, rel_path);

    if (is_file)
        return DeleteFileA(path);
    else
        return RemoveDirectoryA(path);
}

static BOOL delete_cf(const CHAR *rel_path, BOOL is_file)
{
    CHAR path[MAX_PATH];

    lstrcpyA(path, COMMON_FILES_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, rel_path);

    if (is_file)
        return DeleteFileA(path);
    else
        return RemoveDirectoryA(path);
}

static void delete_test_files(void)
{
    DeleteFileA("msitest.msi");
    DeleteFileA("msitest.cab");
    DeleteFileA("msitest\\second\\three.txt");
    DeleteFileA("msitest\\first\\two.txt");
    DeleteFileA("msitest\\one.txt");
    DeleteFileA("msitest\\service.exe");
    DeleteFileA("msitest\\filename");
    RemoveDirectoryA("msitest\\second");
    RemoveDirectoryA("msitest\\first");
    RemoveDirectoryA("msitest");
}

static void write_file(const CHAR *filename, const char *data, int data_size)
{
    DWORD size;

    HANDLE hf = CreateFile(filename, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    WriteFile(hf, data, data_size, &size, NULL);
    CloseHandle(hf);
}

static void write_msi_summary_info(MSIHANDLE db, INT wordcount)
{
    MSIHANDLE summary;
    UINT r;

    r = MsiGetSummaryInformationA(db, NULL, 5, &summary);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiSummaryInfoSetPropertyA(summary, PID_TEMPLATE, VT_LPSTR, 0, NULL, ";1033");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiSummaryInfoSetPropertyA(summary, PID_REVNUMBER, VT_LPSTR, 0, NULL,
                                   "{004757CA-5092-49c2-AD20-28E1CE0DF5F2}");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiSummaryInfoSetPropertyA(summary, PID_PAGECOUNT, VT_I4, 100, NULL, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiSummaryInfoSetPropertyA(summary, PID_WORDCOUNT, VT_I4, wordcount, NULL, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiSummaryInfoSetPropertyA(summary, PID_TITLE, VT_LPSTR, 0, NULL, "MSITEST");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    /* write the summary changes back to the stream */
    r = MsiSummaryInfoPersist(summary);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    MsiCloseHandle(summary);
}

#define create_database(name, tables, num_tables) \
    create_database_wordcount(name, tables, num_tables, 0);

static void create_database_wordcount(const CHAR *name, const msi_table *tables,
                                      int num_tables, INT wordcount)
{
    MSIHANDLE db;
    UINT r;
    int j;

    r = MsiOpenDatabaseA(name, MSIDBOPEN_CREATE, &db);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    /* import the tables into the database */
    for (j = 0; j < num_tables; j++)
    {
        const msi_table *table = &tables[j];

        write_file(table->filename, table->data, (table->size - 1) * sizeof(char));

        r = MsiDatabaseImportA(db, CURR_DIR, table->filename);
        ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

        DeleteFileA(table->filename);
    }

    write_msi_summary_info(db, wordcount);

    r = MsiDatabaseCommit(db);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    MsiCloseHandle(db);
}

static void check_service_is_installed(void)
{
    SC_HANDLE scm, service;
    BOOL res;

    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    ok(scm != NULL, "Failed to open the SC Manager\n");

    service = OpenService(scm, "TestService", SC_MANAGER_ALL_ACCESS);
    ok(service != NULL, "Failed to open TestService\n");

    res = DeleteService(service);
    ok(res, "Failed to delete TestService\n");

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

static BOOL notify_system_change(DWORD event_type, STATEMGRSTATUS *status)
{
    RESTOREPOINTINFOA spec;

    spec.dwEventType = event_type;
    spec.dwRestorePtType = APPLICATION_INSTALL;
    spec.llSequenceNumber = status->llSequenceNumber;
    lstrcpyA(spec.szDescription, "msitest restore point");

    return pSRSetRestorePointA(&spec, status);
}

static void remove_restore_point(DWORD seq_number)
{
    DWORD res;

    res = pSRRemoveRestorePoint(seq_number);
    if (res != ERROR_SUCCESS)
        trace("Failed to remove the restore point : %08x\n", res);
}

static LONG delete_key( HKEY key, LPCSTR subkey, REGSAM access )
{
    if (pRegDeleteKeyExA)
        return pRegDeleteKeyExA( key, subkey, access, 0 );
    return RegDeleteKeyA( key, subkey );
}

static void test_MsiInstallProduct(void)
{
    UINT r;
    CHAR path[MAX_PATH];
    LONG res;
    HKEY hkey;
    DWORD num, size, type;
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    if (on_win9x)
    {
        win_skip("Services are not implemented on Win9x and WinMe\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    /* szPackagePath is NULL */
    r = MsiInstallProductA(NULL, "INSTALL=ALL");
    ok(r == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n", r);

    /* both szPackagePath and szCommandLine are NULL */
    r = MsiInstallProductA(NULL, NULL);
    ok(r == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n", r);

    /* szPackagePath is empty */
    r = MsiInstallProductA("", "INSTALL=ALL");
    ok(r == ERROR_PATH_NOT_FOUND,
       "Expected ERROR_PATH_NOT_FOUND, got %d\n", r);

    create_test_files();
    create_database(msifile, tables, sizeof(tables) / sizeof(msi_table));

    /* install, don't publish */
    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    size = MAX_PATH;
    type = REG_SZ;
    res = RegQueryValueExA(hkey, "Name", NULL, &type, (LPBYTE)path, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(!lstrcmpA(path, "imaname"), "Expected imaname, got %s\n", path);

    size = MAX_PATH;
    type = REG_SZ;
    res = RegQueryValueExA(hkey, "blah", NULL, &type, (LPBYTE)path, &size);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    size = sizeof(num);
    type = REG_DWORD;
    res = RegQueryValueExA(hkey, "number", NULL, &type, (LPBYTE)&num, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(num == 314, "Expected 314, got %d\n", num);

    size = MAX_PATH;
    type = REG_SZ;
    res = RegQueryValueExA(hkey, "OrderTestName", NULL, &type, (LPBYTE)path, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(!lstrcmpA(path, "OrderTestValue"), "Expected OrderTestValue, got %s\n", path);

    check_service_is_installed();

    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", access);

    /* not published, reinstall */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", access);

    create_database(msifile, up_tables, sizeof(up_tables) / sizeof(msi_table));

    /* not published, RemovePreviousVersions set */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", access);

    create_database(msifile, up2_tables, sizeof(up2_tables) / sizeof(msi_table));

    /* not published, version number bumped */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", access);

    create_database(msifile, up3_tables, sizeof(up3_tables) / sizeof(msi_table));

    /* not published, RemovePreviousVersions set and version number bumped */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", access);

    create_database(msifile, up4_tables, sizeof(up4_tables) / sizeof(msi_table));

    /* install, publish product */
    r = MsiInstallProductA(msifile, "PUBLISH_PRODUCT=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    create_database(msifile, up4_tables, sizeof(up4_tables) / sizeof(msi_table));

    /* published, reinstall */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    create_database(msifile, up5_tables, sizeof(up5_tables) / sizeof(msi_table));

    /* published product, RemovePreviousVersions set */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    create_database(msifile, up6_tables, sizeof(up6_tables) / sizeof(msi_table));

    /* published product, version number bumped */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    create_database(msifile, up7_tables, sizeof(up7_tables) / sizeof(msi_table));

    /* published product, RemovePreviousVersions set and version number bumped */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

error:
    delete_test_files();
    DeleteFileA(msifile);
}

static void test_MsiSetComponentState(void)
{
    INSTALLSTATE installed, action;
    MSIHANDLE package;
    char path[MAX_PATH];
    UINT r;

    create_database(msifile, tables, sizeof(tables) / sizeof(msi_table));

    CoInitialize(NULL);

    lstrcpy(path, CURR_DIR);
    lstrcat(path, "\\");
    lstrcat(path, msifile);

    r = MsiOpenPackage(path, &package);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiDoAction(package, "CostInitialize");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiDoAction(package, "FileCost");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiDoAction(package, "CostFinalize");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiGetComponentState(package, "dangler", &installed, &action);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(installed == INSTALLSTATE_ABSENT, "Expected INSTALLSTATE_ABSENT, got %d\n", installed);
    ok(action == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", action);

    r = MsiSetComponentState(package, "dangler", INSTALLSTATE_SOURCE);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    MsiCloseHandle(package);

error:
    CoUninitialize();
    DeleteFileA(msifile);
}

static void test_packagecoltypes(void)
{
    MSIHANDLE hdb, view, rec;
    char path[MAX_PATH];
    LPCSTR query;
    UINT r, count;

    create_database(msifile, tables, sizeof(tables) / sizeof(msi_table));

    CoInitialize(NULL);

    lstrcpy(path, CURR_DIR);
    lstrcat(path, "\\");
    lstrcat(path, msifile);

    r = MsiOpenDatabase(path, MSIDBOPEN_READONLY, &hdb);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    query = "SELECT * FROM `Media`";
    r = MsiDatabaseOpenView( hdb, query, &view );
    ok(r == ERROR_SUCCESS, "MsiDatabaseOpenView failed\n");

    r = MsiViewGetColumnInfo( view, MSICOLINFO_NAMES, &rec );
    count = MsiRecordGetFieldCount( rec );
    ok(r == ERROR_SUCCESS, "MsiViewGetColumnInfo failed\n");
    ok(count == 6, "Expected 6, got %d\n", count);
    ok(check_record(rec, 1, "DiskId"), "wrong column label\n");
    ok(check_record(rec, 2, "LastSequence"), "wrong column label\n");
    ok(check_record(rec, 3, "DiskPrompt"), "wrong column label\n");
    ok(check_record(rec, 4, "Cabinet"), "wrong column label\n");
    ok(check_record(rec, 5, "VolumeLabel"), "wrong column label\n");
    ok(check_record(rec, 6, "Source"), "wrong column label\n");
    MsiCloseHandle(rec);

    r = MsiViewGetColumnInfo( view, MSICOLINFO_TYPES, &rec );
    count = MsiRecordGetFieldCount( rec );
    ok(r == ERROR_SUCCESS, "MsiViewGetColumnInfo failed\n");
    ok(count == 6, "Expected 6, got %d\n", count);
    ok(check_record(rec, 1, "i2"), "wrong column label\n");
    ok(check_record(rec, 2, "i4"), "wrong column label\n");
    ok(check_record(rec, 3, "L64"), "wrong column label\n");
    ok(check_record(rec, 4, "S255"), "wrong column label\n");
    ok(check_record(rec, 5, "S32"), "wrong column label\n");
    ok(check_record(rec, 6, "S72"), "wrong column label\n");

    MsiCloseHandle(rec);
    MsiCloseHandle(view);
    MsiCloseHandle(hdb);
    CoUninitialize();

    DeleteFile(msifile);
}

static void create_cc_test_files(void)
{
    CCAB cabParams;
    HFCI hfci;
    ERF erf;
    static CHAR cab_context[] = "test%d.cab";
    BOOL res;

    create_file("maximus", 500);
    create_file("augustus", 50000);
    create_file("tiberius", 500);
    create_file("caesar", 500);

    set_cab_parameters(&cabParams, "test1.cab", 40000);

    hfci = FCICreate(&erf, file_placed, mem_alloc, mem_free, fci_open,
                      fci_read, fci_write, fci_close, fci_seek, fci_delete,
                      get_temp_file, &cabParams, cab_context);
    ok(hfci != NULL, "Failed to create an FCI context\n");

    res = add_file(hfci, "maximus", tcompTYPE_NONE);
    ok(res, "Failed to add file maximus\n");

    res = add_file(hfci, "augustus", tcompTYPE_NONE);
    ok(res, "Failed to add file augustus\n");

    res = add_file(hfci, "tiberius", tcompTYPE_NONE);
    ok(res, "Failed to add file tiberius\n");

    res = FCIFlushCabinet(hfci, FALSE, get_next_cabinet, progress);
    ok(res, "Failed to flush the cabinet\n");

    res = FCIDestroy(hfci);
    ok(res, "Failed to destroy the cabinet\n");

    create_cab_file("test3.cab", MEDIA_SIZE, "caesar\0");

    DeleteFile("maximus");
    DeleteFile("augustus");
    DeleteFile("tiberius");
    DeleteFile("caesar");
}

static void delete_cab_files(void)
{
    SHFILEOPSTRUCT shfl;
    CHAR path[MAX_PATH+10];

    lstrcpyA(path, CURR_DIR);
    lstrcatA(path, "\\*.cab");
    path[strlen(path) + 1] = '\0';

    shfl.hwnd = NULL;
    shfl.wFunc = FO_DELETE;
    shfl.pFrom = path;
    shfl.pTo = NULL;
    shfl.fFlags = FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NORECURSION | FOF_SILENT;

    SHFileOperation(&shfl);
}

static void test_continuouscabs(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_cc_test_files();
    create_database(msifile, cc_tables, sizeof(cc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_SUCCESS) /* win9x has a problem with this */
    {
        ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
        ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
        ok(delete_pf("msitest", FALSE), "File not installed\n");
    }

    delete_cab_files();
    DeleteFile(msifile);

    create_cc_test_files();
    create_database(msifile, cc2_tables, sizeof(cc2_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest\\tiberius", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    delete_cab_files();
    DeleteFile(msifile);
}

static void test_caborder(void)
{
    UINT r;

    create_file("imperator", 100);
    create_file("maximus", 500);
    create_file("augustus", 50000);
    create_file("caesar", 500);

    create_database(msifile, cc_tables, sizeof(cc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    create_cab_file("test1.cab", MEDIA_SIZE, "maximus\0");
    create_cab_file("test2.cab", MEDIA_SIZE, "augustus\0");
    create_cab_file("test3.cab", MEDIA_SIZE, "caesar\0");

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    ok(!delete_pf("msitest\\augustus", TRUE), "File is installed\n");
    ok(!delete_pf("msitest\\caesar", TRUE), "File is installed\n");
    todo_wine
    {
        ok(!delete_pf("msitest\\maximus", TRUE), "File is installed\n");
        ok(!delete_pf("msitest", FALSE), "File is installed\n");
    }

    delete_cab_files();

    create_cab_file("test1.cab", MEDIA_SIZE, "imperator\0");
    create_cab_file("test2.cab", MEDIA_SIZE, "maximus\0augustus\0");
    create_cab_file("test3.cab", MEDIA_SIZE, "caesar\0");

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    ok(!delete_pf("msitest\\maximus", TRUE), "File is installed\n");
    ok(!delete_pf("msitest\\augustus", TRUE), "File is installed\n");
    ok(!delete_pf("msitest\\caesar", TRUE), "File is installed\n");
    ok(!delete_pf("msitest", FALSE), "File is installed\n");

    delete_cab_files();
    DeleteFile(msifile);

    create_cc_test_files();
    create_database(msifile, co_tables, sizeof(co_tables) / sizeof(msi_table));

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    ok(!delete_pf("msitest\\caesar", TRUE), "File is installed\n");
    ok(!delete_pf("msitest", FALSE), "File is installed\n");
    todo_wine
    {
        ok(!delete_pf("msitest\\augustus", TRUE), "File is installed\n");
        ok(!delete_pf("msitest\\maximus", TRUE), "File is installed\n");
    }

    delete_cab_files();
    DeleteFile(msifile);

    create_cc_test_files();
    create_database(msifile, co2_tables, sizeof(co2_tables) / sizeof(msi_table));

    r = MsiInstallProductA(msifile, NULL);
    ok(!delete_pf("msitest\\caesar", TRUE), "File is installed\n");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    todo_wine
    {
        ok(!delete_pf("msitest\\augustus", TRUE), "File is installed\n");
        ok(!delete_pf("msitest\\maximus", TRUE), "File is installed\n");
        ok(!delete_pf("msitest", FALSE), "File is installed\n");
    }

error:
    delete_cab_files();
    DeleteFile("imperator");
    DeleteFile("maximus");
    DeleteFile("augustus");
    DeleteFile("caesar");
    DeleteFile(msifile);
}

static void test_mixedmedia(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);
    create_file("msitest\\augustus", 500);
    create_file("caesar", 500);

    create_database(msifile, mm_tables, sizeof(mm_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    create_cab_file("test1.cab", MEDIA_SIZE, "caesar\0");

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    /* Delete the files in the temp (current) folder */
    DeleteFile("msitest\\maximus");
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
    DeleteFile("caesar");
    DeleteFile("test1.cab");
    DeleteFile(msifile);
}

static void test_samesequence(void)
{
    UINT r;

    create_cc_test_files();
    create_database(msifile, ss_tables, sizeof(ss_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_SUCCESS) /* win9x has a problem with this */
    {
        ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
        ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
        ok(delete_pf("msitest", FALSE), "File not installed\n");
    }

    delete_cab_files();
    DeleteFile(msifile);
}

static void test_uiLevelFlags(void)
{
    UINT r;

    create_cc_test_files();
    create_database(msifile, ui_tables, sizeof(ui_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE | INSTALLUILEVEL_SOURCERESONLY, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_SUCCESS) /* win9x has a problem with this */
    {
        ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
        ok(!delete_pf("msitest\\maximus", TRUE), "UI install occurred, but execute-only was requested.\n");
        ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
        ok(delete_pf("msitest", FALSE), "File not installed\n");
    }

    delete_cab_files();
    DeleteFile(msifile);
}

static BOOL file_matches(LPSTR path)
{
    CHAR buf[MAX_PATH];
    HANDLE file;
    DWORD size;

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, OPEN_EXISTING, 0, NULL);

    ZeroMemory(buf, MAX_PATH);
    ReadFile(file, buf, 15, &size, NULL);
    CloseHandle(file);

    return !lstrcmp(buf, "msitest\\maximus");
}

static void test_readonlyfile(void)
{
    UINT r;
    DWORD size;
    HANDLE file;
    CHAR path[MAX_PATH];

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);
    create_database(msifile, rof_tables, sizeof(rof_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    lstrcpy(path, PROG_FILES_DIR);
    lstrcat(path, "\\msitest");
    CreateDirectory(path, NULL);

    lstrcat(path, "\\maximus");
    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, CREATE_NEW, FILE_ATTRIBUTE_READONLY, NULL);

    WriteFile(file, "readonlyfile", strlen("readonlyfile"), &size, NULL);
    CloseHandle(file);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(file_matches(path), "Expected file to be overwritten\n");
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    /* Delete the files in the temp (current) folder */
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    DeleteFile(msifile);
}

static void test_readonlyfile_cab(void)
{
    UINT r;
    DWORD size;
    HANDLE file;
    CHAR path[MAX_PATH];
    CHAR buf[16];

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("maximus", 500);
    create_cab_file("test1.cab", MEDIA_SIZE, "maximus\0");
    DeleteFile("maximus");

    create_database(msifile, rofc_tables, sizeof(rofc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    lstrcpy(path, PROG_FILES_DIR);
    lstrcat(path, "\\msitest");
    CreateDirectory(path, NULL);

    lstrcat(path, "\\maximus");
    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, CREATE_NEW, FILE_ATTRIBUTE_READONLY, NULL);

    WriteFile(file, "readonlyfile", strlen("readonlyfile"), &size, NULL);
    CloseHandle(file);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    memset( buf, 0, sizeof(buf) );
    if ((file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
    {
        ReadFile(file, buf, sizeof(buf) - 1, &size, NULL);
        CloseHandle(file);
    }
    ok(!memcmp( buf, "maximus", sizeof("maximus")-1 ), "Expected file to be overwritten, got '%s'\n", buf);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    /* Delete the files in the temp (current) folder */
    delete_cab_files();
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    DeleteFile(msifile);
}

static BOOL add_cabinet_storage(LPCSTR db, LPCSTR cabinet)
{
    WCHAR dbW[MAX_PATH], cabinetW[MAX_PATH];
    IStorage *stg;
    IStream *stm;
    HRESULT hr;
    HANDLE handle;

    MultiByteToWideChar(CP_ACP, 0, db, -1, dbW, MAX_PATH);
    hr = StgOpenStorage(dbW, NULL, STGM_DIRECT|STGM_READWRITE|STGM_SHARE_EXCLUSIVE, NULL, 0, &stg);
    if (FAILED(hr))
        return FALSE;

    MultiByteToWideChar(CP_ACP, 0, cabinet, -1, cabinetW, MAX_PATH);
    hr = IStorage_CreateStream(stg, cabinetW, STGM_WRITE|STGM_SHARE_EXCLUSIVE, 0, 0, &stm);
    if (FAILED(hr))
    {
        IStorage_Release(stg);
        return FALSE;
    }

    handle = CreateFileW(cabinetW, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (handle != INVALID_HANDLE_VALUE)
    {
        DWORD count;
        char buffer[1024];
        if (ReadFile(handle, buffer, sizeof(buffer), &count, NULL))
            IStream_Write(stm, buffer, count, &count);
        CloseHandle(handle);
    }

    IStream_Release(stm);
    IStorage_Release(stg);

    return TRUE;
}

static void test_lastusedsource(void)
{
    static char prodcode[] = "{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}";

    UINT r;
    char value[MAX_PATH], path[MAX_PATH];
    DWORD size;

    if (!pMsiSourceListGetInfoA)
    {
        win_skip("MsiSourceListGetInfoA is not available\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("maximus", 500);
    create_cab_file("test1.cab", MEDIA_SIZE, "maximus\0");
    DeleteFile("maximus");

    create_database("msifile0.msi", lus0_tables, sizeof(lus0_tables) / sizeof(msi_table));
    create_database("msifile1.msi", lus1_tables, sizeof(lus1_tables) / sizeof(msi_table));
    create_database("msifile2.msi", lus2_tables, sizeof(lus2_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* no cabinet file */

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %u\n", r);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    r = MsiInstallProductA("msifile0.msi", "PUBLISH_PRODUCT=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    lstrcpyA(path, CURR_DIR);
    lstrcatA(path, "\\");

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    todo_wine
    {
    ok(!lstrcmpA(value, path), "Expected \"%s\", got \"%s\"\n", path, value);
    ok(size == lstrlenA(path), "Expected %d, got %d\n", lstrlenA(path), size);
    }

    r = MsiInstallProductA("msifile0.msi", "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    /* separate cabinet file */

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %u\n", r);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    r = MsiInstallProductA("msifile1.msi", "PUBLISH_PRODUCT=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    lstrcpyA(path, CURR_DIR);
    lstrcatA(path, "\\");

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    todo_wine
    {
    ok(!lstrcmpA(value, path), "Expected \"%s\", got \"%s\"\n", path, value);
    ok(size == lstrlenA(path), "Expected %d, got %d\n", lstrlenA(path), size);
    }

    r = MsiInstallProductA("msifile1.msi", "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %u\n", r);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    /* embedded cabinet stream */

    add_cabinet_storage("msifile2.msi", "test1.cab");

    r = MsiInstallProductA("msifile2.msi", "PUBLISH_PRODUCT=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    todo_wine
    {
    ok(!lstrcmpA(value, path), "Expected \"%s\", got \"%s\"\n", path, value);
    ok(size == lstrlenA(path), "Expected %d, got %d\n", lstrlenA(path), size);
    }

    r = MsiInstallProductA("msifile2.msi", "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %u\n", r);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

error:
    /* Delete the files in the temp (current) folder */
    delete_cab_files();
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    DeleteFile("msifile0.msi");
    DeleteFile("msifile1.msi");
    DeleteFile("msifile2.msi");
}

static void test_setdirproperty(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);
    create_database(msifile, sdp_tables, sizeof(sdp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_cf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_cf("msitest", FALSE), "File not installed\n");

error:
    /* Delete the files in the temp (current) folder */
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
}

static void test_cabisextracted(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\gaius", 500);
    create_file("maximus", 500);
    create_file("augustus", 500);
    create_file("caesar", 500);

    create_cab_file("test1.cab", MEDIA_SIZE, "maximus\0");
    create_cab_file("test2.cab", MEDIA_SIZE, "augustus\0");
    create_cab_file("test3.cab", MEDIA_SIZE, "caesar\0");

    create_database(msifile, cie_tables, sizeof(cie_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\gaius", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    /* Delete the files in the temp (current) folder */
    delete_cab_files();
    DeleteFile(msifile);
    DeleteFile("maximus");
    DeleteFile("augustus");
    DeleteFile("caesar");
    DeleteFile("msitest\\gaius");
    RemoveDirectory("msitest");
}

static void test_concurrentinstall(void)
{
    UINT r;
    CHAR path[MAX_PATH];

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    CreateDirectoryA("msitest\\msitest", NULL);
    create_file("msitest\\maximus", 500);
    create_file("msitest\\msitest\\augustus", 500);

    create_database(msifile, ci_tables, sizeof(ci_tables) / sizeof(msi_table));

    lstrcpyA(path, CURR_DIR);
    lstrcatA(path, "\\msitest\\concurrent.msi");
    create_database(path, ci2_tables, sizeof(ci2_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        DeleteFile(path);
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    if (!delete_pf("msitest\\augustus", TRUE))
        trace("concurrent installs not supported\n");
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    DeleteFile(path);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\msitest\\augustus");
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest\\msitest");
    RemoveDirectory("msitest");
}

static void test_setpropertyfolder(void)
{
    UINT r;
    CHAR path[MAX_PATH];
    DWORD attr;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\msitest\\added");

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, spf_tables, sizeof(spf_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ok(delete_pf("msitest\\added\\maximus", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\added", FALSE), "File not installed\n");
        ok(delete_pf("msitest", FALSE), "File not installed\n");
    }
    else
    {
        trace("changing folder property not supported\n");
        ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
        ok(delete_pf("msitest", FALSE), "File not installed\n");
    }

error:
    /* Delete the files in the temp (current) folder */
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
}

static BOOL file_exists(LPCSTR file)
{
    return GetFileAttributes(file) != INVALID_FILE_ATTRIBUTES;
}

static BOOL pf_exists(LPCSTR file)
{
    CHAR path[MAX_PATH];

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, file);

    return file_exists(path);
}

static void delete_pfmsitest_files(void)
{
    SHFILEOPSTRUCT shfl;
    CHAR path[MAX_PATH+11];

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\msitest\\*");
    path[strlen(path) + 1] = '\0';

    shfl.hwnd = NULL;
    shfl.wFunc = FO_DELETE;
    shfl.pFrom = path;
    shfl.pTo = NULL;
    shfl.fFlags = FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_NORECURSION | FOF_SILENT;

    SHFileOperation(&shfl);

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\msitest");
    RemoveDirectoryA(path);
}

static void check_reg_str(HKEY prodkey, LPCSTR name, LPCSTR expected, BOOL bcase, DWORD line)
{
    char val[MAX_PATH];
    DWORD size, type;
    LONG res;

    size = MAX_PATH;
    val[0] = '\0';
    res = RegQueryValueExA(prodkey, name, NULL, &type, (LPBYTE)val, &size);

    if (res != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ))
    {
        ok_(__FILE__, line)(FALSE, "Key doesn't exist or wrong type\n");
        return;
    }

    if (!expected)
        ok_(__FILE__, line)(lstrlenA(val) == 0, "Expected empty string, got %s\n", val);
    else
    {
        if (bcase)
            ok_(__FILE__, line)(!lstrcmpA(val, expected), "Expected %s, got %s\n", expected, val);
        else
            ok_(__FILE__, line)(!lstrcmpiA(val, expected), "Expected %s, got %s\n", expected, val);
    }
}

static void check_reg_dword(HKEY prodkey, LPCSTR name, DWORD expected, DWORD line)
{
    DWORD val, size, type;
    LONG res;

    size = sizeof(DWORD);
    res = RegQueryValueExA(prodkey, name, NULL, &type, (LPBYTE)&val, &size);

    if (res != ERROR_SUCCESS || type != REG_DWORD)
    {
        ok_(__FILE__, line)(FALSE, "Key doesn't exist or wrong type\n");
        return;
    }

    ok_(__FILE__, line)(val == expected, "Expected %d, got %d\n", expected, val);
}

static void check_reg_dword2(HKEY prodkey, LPCSTR name, DWORD expected1, DWORD expected2, DWORD line)
{
    DWORD val, size, type;
    LONG res;

    size = sizeof(DWORD);
    res = RegQueryValueExA(prodkey, name, NULL, &type, (LPBYTE)&val, &size);

    if (res != ERROR_SUCCESS || type != REG_DWORD)
    {
        ok_(__FILE__, line)(FALSE, "Key doesn't exist or wrong type\n");
        return;
    }

    ok_(__FILE__, line)(val == expected1 || val == expected2, "Expected %d or %d, got %d\n", expected1, expected2, val);
}

static void check_reg_dword3(HKEY prodkey, LPCSTR name, DWORD expected1, DWORD expected2, DWORD expected3, DWORD line)
{
    DWORD val, size, type;
    LONG res;

    size = sizeof(DWORD);
    res = RegQueryValueExA(prodkey, name, NULL, &type, (LPBYTE)&val, &size);

    if (res != ERROR_SUCCESS || type != REG_DWORD)
    {
        ok_(__FILE__, line)(FALSE, "Key doesn't exist or wrong type\n");
        return;
    }

    ok_(__FILE__, line)(val == expected1 || val == expected2 || val == expected3,
                        "Expected %d, %d or %d, got %d\n", expected1, expected2, expected3, val);
}

#define CHECK_REG_STR(prodkey, name, expected) \
    check_reg_str(prodkey, name, expected, TRUE, __LINE__);

#define CHECK_DEL_REG_STR(prodkey, name, expected) \
    check_reg_str(prodkey, name, expected, TRUE, __LINE__); \
    RegDeleteValueA(prodkey, name);

#define CHECK_REG_ISTR(prodkey, name, expected) \
    check_reg_str(prodkey, name, expected, FALSE, __LINE__);

#define CHECK_DEL_REG_ISTR(prodkey, name, expected) \
    check_reg_str(prodkey, name, expected, FALSE, __LINE__); \
    RegDeleteValueA(prodkey, name);

#define CHECK_REG_DWORD(prodkey, name, expected) \
    check_reg_dword(prodkey, name, expected, __LINE__);

#define CHECK_DEL_REG_DWORD(prodkey, name, expected) \
    check_reg_dword(prodkey, name, expected, __LINE__); \
    RegDeleteValueA(prodkey, name);

#define CHECK_REG_DWORD2(prodkey, name, expected1, expected2) \
    check_reg_dword2(prodkey, name, expected1, expected2, __LINE__);

#define CHECK_DEL_REG_DWORD2(prodkey, name, expected1, expected2) \
    check_reg_dword2(prodkey, name, expected1, expected2, __LINE__); \
    RegDeleteValueA(prodkey, name);

#define CHECK_REG_DWORD3(prodkey, name, expected1, expected2, expected3) \
    check_reg_dword3(prodkey, name, expected1, expected2, expected3, __LINE__);

#define CHECK_DEL_REG_DWORD3(prodkey, name, expected1, expected2, expected3) \
    check_reg_dword3(prodkey, name, expected1, expected2, expected3, __LINE__); \
    RegDeleteValueA(prodkey, name);

static void get_date_str(LPSTR date)
{
    SYSTEMTIME systime;

    static const char date_fmt[] = "%d%02d%02d";
    GetLocalTime(&systime);
    sprintf(date, date_fmt, systime.wYear, systime.wMonth, systime.wDay);
}

static void test_publish_registerproduct(void)
{
    UINT r;
    LONG res;
    HKEY hkey;
    HKEY props, usage;
    LPSTR usersid;
    char date[MAX_PATH];
    char temp[MAX_PATH];
    char keypath[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    static const CHAR uninstall[] = "Software\\Microsoft\\Windows\\CurrentVersion"
                                    "\\Uninstall\\{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}";
    static const CHAR userdata[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Installer"
                                   "\\UserData\\%s\\Products\\84A88FD7F6998CE40A22FB59F6B9C2BB";
    static const CHAR ugkey[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Installer"
                                "\\UpgradeCodes\\51AAE0C44620A5E4788506E91F249BD2";
    static const CHAR userugkey[] = "Software\\Microsoft\\Installer\\UpgradeCodes"
                                    "\\51AAE0C44620A5E4788506E91F249BD2";

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    if (!get_user_sid(&usersid))
        return;

    get_date_str(date);
    GetTempPath(MAX_PATH, temp);

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, pp_tables, sizeof(pp_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* RegisterProduct */
    r = MsiInstallProductA(msifile, "REGISTER_PRODUCT=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyA(HKEY_CURRENT_USER, userugkey, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, uninstall, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "DisplayName", "MSITEST");
    CHECK_DEL_REG_STR(hkey, "DisplayVersion", "1.1.1");
    CHECK_DEL_REG_STR(hkey, "InstallDate", date);
    CHECK_DEL_REG_STR(hkey, "InstallSource", temp);
    CHECK_DEL_REG_ISTR(hkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(hkey, "Publisher", "Wine");
    CHECK_DEL_REG_STR(hkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(hkey, "AuthorizedCDFPrefix", NULL);
    CHECK_DEL_REG_STR(hkey, "Comments", NULL);
    CHECK_DEL_REG_STR(hkey, "Contact", NULL);
    CHECK_DEL_REG_STR(hkey, "HelpLink", NULL);
    CHECK_DEL_REG_STR(hkey, "HelpTelephone", NULL);
    CHECK_DEL_REG_STR(hkey, "InstallLocation", NULL);
    CHECK_DEL_REG_STR(hkey, "Readme", NULL);
    CHECK_DEL_REG_STR(hkey, "Size", NULL);
    CHECK_DEL_REG_STR(hkey, "URLInfoAbout", NULL);
    CHECK_DEL_REG_STR(hkey, "URLUpdateInfo", NULL);
    CHECK_DEL_REG_DWORD(hkey, "Language", 1033);
    CHECK_DEL_REG_DWORD(hkey, "Version", 0x1010001);
    CHECK_DEL_REG_DWORD(hkey, "VersionMajor", 1);
    CHECK_DEL_REG_DWORD(hkey, "VersionMinor", 1);
    CHECK_DEL_REG_DWORD(hkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_DEL_REG_DWORD3(hkey, "EstimatedSize", 12, -12, 4);
    }

    delete_key(hkey, "", access);
    RegCloseKey(hkey);

    sprintf(keypath, userdata, usersid);
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegOpenKeyExA(hkey, "InstallProperties", 0, access, &props);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    RegDeleteValueA(props, "LocalPackage"); /* LocalPackage is nondeterministic */
    CHECK_DEL_REG_STR(props, "DisplayName", "MSITEST");
    CHECK_DEL_REG_STR(props, "DisplayVersion", "1.1.1");
    CHECK_DEL_REG_STR(props, "InstallDate", date);
    CHECK_DEL_REG_STR(props, "InstallSource", temp);
    CHECK_DEL_REG_ISTR(props, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(props, "Publisher", "Wine");
    CHECK_DEL_REG_STR(props, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(props, "AuthorizedCDFPrefix", NULL);
    CHECK_DEL_REG_STR(props, "Comments", NULL);
    CHECK_DEL_REG_STR(props, "Contact", NULL);
    CHECK_DEL_REG_STR(props, "HelpLink", NULL);
    CHECK_DEL_REG_STR(props, "HelpTelephone", NULL);
    CHECK_DEL_REG_STR(props, "InstallLocation", NULL);
    CHECK_DEL_REG_STR(props, "Readme", NULL);
    CHECK_DEL_REG_STR(props, "Size", NULL);
    CHECK_DEL_REG_STR(props, "URLInfoAbout", NULL);
    CHECK_DEL_REG_STR(props, "URLUpdateInfo", NULL);
    CHECK_DEL_REG_DWORD(props, "Language", 1033);
    CHECK_DEL_REG_DWORD(props, "Version", 0x1010001);
    CHECK_DEL_REG_DWORD(props, "VersionMajor", 1);
    CHECK_DEL_REG_DWORD(props, "VersionMinor", 1);
    CHECK_DEL_REG_DWORD(props, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_DEL_REG_DWORD3(props, "EstimatedSize", 12, -12, 4);
    }

    delete_key(props, "", access);
    RegCloseKey(props);

    res = RegOpenKeyExA(hkey, "Usage", 0, access, &usage);
    todo_wine
    {
        ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    }

    delete_key(usage, "", access);
    RegCloseKey(usage);
    delete_key(hkey, "", access);
    RegCloseKey(hkey);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, ugkey, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "84A88FD7F6998CE40A22FB59F6B9C2BB", NULL);

    delete_key(hkey, "", access);
    RegCloseKey(hkey);

    /* RegisterProduct, machine */
    r = MsiInstallProductA(msifile, "REGISTER_PRODUCT=1 ALLUSERS=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, userugkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, uninstall, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "DisplayName", "MSITEST");
    CHECK_DEL_REG_STR(hkey, "DisplayVersion", "1.1.1");
    CHECK_DEL_REG_STR(hkey, "InstallDate", date);
    CHECK_DEL_REG_STR(hkey, "InstallSource", temp);
    CHECK_DEL_REG_ISTR(hkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(hkey, "Publisher", "Wine");
    CHECK_DEL_REG_STR(hkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(hkey, "AuthorizedCDFPrefix", NULL);
    CHECK_DEL_REG_STR(hkey, "Comments", NULL);
    CHECK_DEL_REG_STR(hkey, "Contact", NULL);
    CHECK_DEL_REG_STR(hkey, "HelpLink", NULL);
    CHECK_DEL_REG_STR(hkey, "HelpTelephone", NULL);
    CHECK_DEL_REG_STR(hkey, "InstallLocation", NULL);
    CHECK_DEL_REG_STR(hkey, "Readme", NULL);
    CHECK_DEL_REG_STR(hkey, "Size", NULL);
    CHECK_DEL_REG_STR(hkey, "URLInfoAbout", NULL);
    CHECK_DEL_REG_STR(hkey, "URLUpdateInfo", NULL);
    CHECK_DEL_REG_DWORD(hkey, "Language", 1033);
    CHECK_DEL_REG_DWORD(hkey, "Version", 0x1010001);
    CHECK_DEL_REG_DWORD(hkey, "VersionMajor", 1);
    CHECK_DEL_REG_DWORD(hkey, "VersionMinor", 1);
    CHECK_DEL_REG_DWORD(hkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_DEL_REG_DWORD3(hkey, "EstimatedSize", 12, -12, 4);
    }

    delete_key(hkey, "", access);
    RegCloseKey(hkey);

    sprintf(keypath, userdata, "S-1-5-18");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegOpenKeyExA(hkey, "InstallProperties", 0, access, &props);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    RegDeleteValueA(props, "LocalPackage"); /* LocalPackage is nondeterministic */
    CHECK_DEL_REG_STR(props, "DisplayName", "MSITEST");
    CHECK_DEL_REG_STR(props, "DisplayVersion", "1.1.1");
    CHECK_DEL_REG_STR(props, "InstallDate", date);
    CHECK_DEL_REG_STR(props, "InstallSource", temp);
    CHECK_DEL_REG_ISTR(props, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(props, "Publisher", "Wine");
    CHECK_DEL_REG_STR(props, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_DEL_REG_STR(props, "AuthorizedCDFPrefix", NULL);
    CHECK_DEL_REG_STR(props, "Comments", NULL);
    CHECK_DEL_REG_STR(props, "Contact", NULL);
    CHECK_DEL_REG_STR(props, "HelpLink", NULL);
    CHECK_DEL_REG_STR(props, "HelpTelephone", NULL);
    CHECK_DEL_REG_STR(props, "InstallLocation", NULL);
    CHECK_DEL_REG_STR(props, "Readme", NULL);
    CHECK_DEL_REG_STR(props, "Size", NULL);
    CHECK_DEL_REG_STR(props, "URLInfoAbout", NULL);
    CHECK_DEL_REG_STR(props, "URLUpdateInfo", NULL);
    CHECK_DEL_REG_DWORD(props, "Language", 1033);
    CHECK_DEL_REG_DWORD(props, "Version", 0x1010001);
    CHECK_DEL_REG_DWORD(props, "VersionMajor", 1);
    CHECK_DEL_REG_DWORD(props, "VersionMinor", 1);
    CHECK_DEL_REG_DWORD(props, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_DEL_REG_DWORD3(props, "EstimatedSize", 12, -12, 4);
    }

    delete_key(props, "", access);
    RegCloseKey(props);

    res = RegOpenKeyExA(hkey, "Usage", 0, access, &usage);
    todo_wine
    {
        ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    }

    delete_key(usage, "", access);
    RegCloseKey(usage);
    delete_key(hkey, "", access);
    RegCloseKey(hkey);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, ugkey, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "84A88FD7F6998CE40A22FB59F6B9C2BB", NULL);

    delete_key(hkey, "", access);
    RegCloseKey(hkey);

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    HeapFree(GetProcessHeap(), 0, usersid);
}

static void test_publish_publishproduct(void)
{
    UINT r;
    LONG res;
    LPSTR usersid;
    HKEY sourcelist, net, props;
    HKEY hkey, patches, media;
    CHAR keypath[MAX_PATH];
    CHAR temp[MAX_PATH];
    CHAR path[MAX_PATH];
    BOOL wow64, old_installer = FALSE;
    REGSAM access = KEY_ALL_ACCESS;

    static const CHAR prodpath[] = "Software\\Microsoft\\Windows\\CurrentVersion"
                                   "\\Installer\\UserData\\%s\\Products"
                                   "\\84A88FD7F6998CE40A22FB59F6B9C2BB";
    static const CHAR cuprodpath[] = "Software\\Microsoft\\Installer\\Products"
                                     "\\84A88FD7F6998CE40A22FB59F6B9C2BB";
    static const CHAR cuupgrades[] = "Software\\Microsoft\\Installer\\UpgradeCodes"
                                     "\\51AAE0C44620A5E4788506E91F249BD2";
    static const CHAR badprod[] = "Software\\Microsoft\\Windows\\CurrentVersion"
                                  "\\Installer\\Products"
                                  "\\84A88FD7F6998CE40A22FB59F6B9C2BB";
    static const CHAR machprod[] = "Installer\\Products\\84A88FD7F6998CE40A22FB59F6B9C2BB";
    static const CHAR machup[] = "Installer\\UpgradeCodes\\51AAE0C44620A5E4788506E91F249BD2";

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    if (!get_user_sid(&usersid))
        return;

    GetTempPath(MAX_PATH, temp);

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, pp_tables, sizeof(pp_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* PublishProduct, current user */
    r = MsiInstallProductA(msifile, "PUBLISH_PRODUCT=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, badprod, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    sprintf(keypath, prodpath, usersid);
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &hkey);
    if (res == ERROR_FILE_NOT_FOUND)
    {
        res = RegOpenKeyA(HKEY_CURRENT_USER, cuprodpath, &hkey);
        if (res == ERROR_SUCCESS)
        {
            win_skip("Windows Installer < 3.0 detected\n");
            RegCloseKey(hkey);
            old_installer = TRUE;
            goto currentuser;
        }
        else
        {
            win_skip("Install failed, no need to continue\n");
            return;
        }
    }
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegOpenKeyExA(hkey, "InstallProperties", 0, access, &props);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyExA(hkey, "Patches", 0, access, &patches);
    todo_wine
    {
        ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
        CHECK_DEL_REG_STR(patches, "AllPatches", NULL);
    }

    delete_key(patches, "", access);
    RegCloseKey(patches);
    delete_key(hkey, "", access);
    RegCloseKey(hkey);

currentuser:
    res = RegOpenKeyA(HKEY_CURRENT_USER, cuprodpath, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "ProductName", "MSITEST");
    CHECK_DEL_REG_STR(hkey, "PackageCode", "AC75740029052c94DA02821EECD05F2F");
    CHECK_DEL_REG_DWORD(hkey, "Language", 1033);
    CHECK_DEL_REG_DWORD(hkey, "Version", 0x1010001);
    if (!old_installer)
        CHECK_DEL_REG_DWORD(hkey, "AuthorizedLUAApp", 0);
    CHECK_DEL_REG_DWORD(hkey, "Assignment", 0);
    CHECK_DEL_REG_DWORD(hkey, "AdvertiseFlags", 0x184);
    CHECK_DEL_REG_DWORD(hkey, "InstanceType", 0);
    CHECK_DEL_REG_STR(hkey, "Clients", ":");

    res = RegOpenKeyA(hkey, "SourceList", &sourcelist);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    lstrcpyA(path, "n;1;");
    lstrcatA(path, temp);
    CHECK_DEL_REG_STR(sourcelist, "LastUsedSource", path);
    CHECK_DEL_REG_STR(sourcelist, "PackageName", "msitest.msi");

    res = RegOpenKeyA(sourcelist, "Net", &net);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(net, "1", temp);

    RegDeleteKeyA(net, "");
    RegCloseKey(net);

    res = RegOpenKeyA(sourcelist, "Media", &media);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(media, "1", "DISK1;");

    RegDeleteKeyA(media, "");
    RegCloseKey(media);
    RegDeleteKeyA(sourcelist, "");
    RegCloseKey(sourcelist);
    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

    res = RegOpenKeyA(HKEY_CURRENT_USER, cuupgrades, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "84A88FD7F6998CE40A22FB59F6B9C2BB", NULL);

    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

    /* PublishProduct, machine */
    r = MsiInstallProductA(msifile, "PUBLISH_PRODUCT=1 ALLUSERS=1");
    if (old_installer)
        goto machprod;
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, badprod, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    sprintf(keypath, prodpath, "S-1-5-18");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegOpenKeyExA(hkey, "InstallProperties", 0, access, &props);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyExA(hkey, "Patches", 0, access, &patches);
    todo_wine
    {
        ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
        CHECK_DEL_REG_STR(patches, "AllPatches", NULL);
    }

    delete_key(patches, "", access);
    RegCloseKey(patches);
    delete_key(hkey, "", access);
    RegCloseKey(hkey);

machprod:
    res = RegOpenKeyA(HKEY_CLASSES_ROOT, machprod, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "ProductName", "MSITEST");
    CHECK_DEL_REG_STR(hkey, "PackageCode", "AC75740029052c94DA02821EECD05F2F");
    CHECK_DEL_REG_DWORD(hkey, "Language", 1033);
    CHECK_DEL_REG_DWORD(hkey, "Version", 0x1010001);
    if (!old_installer)
        CHECK_DEL_REG_DWORD(hkey, "AuthorizedLUAApp", 0);
    todo_wine CHECK_DEL_REG_DWORD(hkey, "Assignment", 1);
    CHECK_DEL_REG_DWORD(hkey, "AdvertiseFlags", 0x184);
    CHECK_DEL_REG_DWORD(hkey, "InstanceType", 0);
    CHECK_DEL_REG_STR(hkey, "Clients", ":");

    res = RegOpenKeyA(hkey, "SourceList", &sourcelist);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    lstrcpyA(path, "n;1;");
    lstrcatA(path, temp);
    CHECK_DEL_REG_STR(sourcelist, "LastUsedSource", path);
    CHECK_DEL_REG_STR(sourcelist, "PackageName", "msitest.msi");

    res = RegOpenKeyA(sourcelist, "Net", &net);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(net, "1", temp);

    RegDeleteKeyA(net, "");
    RegCloseKey(net);

    res = RegOpenKeyA(sourcelist, "Media", &media);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(media, "1", "DISK1;");

    RegDeleteKeyA(media, "");
    RegCloseKey(media);
    RegDeleteKeyA(sourcelist, "");
    RegCloseKey(sourcelist);
    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, machup, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_DEL_REG_STR(hkey, "84A88FD7F6998CE40A22FB59F6B9C2BB", NULL);

    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    HeapFree(GetProcessHeap(), 0, usersid);
}

static void test_publish_publishfeatures(void)
{
    UINT r;
    LONG res;
    HKEY hkey;
    LPSTR usersid;
    CHAR keypath[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    static const CHAR cupath[] = "Software\\Microsoft\\Installer\\Features"
                                 "\\84A88FD7F6998CE40A22FB59F6B9C2BB";
    static const CHAR udpath[] = "Software\\Microsoft\\Windows\\CurrentVersion"
                                 "\\Installer\\UserData\\%s\\Products"
                                 "\\84A88FD7F6998CE40A22FB59F6B9C2BB\\Features";
    static const CHAR featkey[] = "Software\\Microsoft\\Windows\\CurrentVersion"
                                  "\\Installer\\Features";
    static const CHAR classfeat[] = "Software\\Classes\\Installer\\Features"
                                    "\\84A88FD7F6998CE40A22FB59F6B9C2BB";

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    if (!get_user_sid(&usersid))
        return;

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, pp_tables, sizeof(pp_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* PublishFeatures, current user */
    r = MsiInstallProductA(msifile, "PUBLISH_FEATURES=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, featkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, classfeat, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyA(HKEY_CURRENT_USER, cupath, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(hkey, "feature", "");
    CHECK_REG_STR(hkey, "montecristo", "");

    RegDeleteValueA(hkey, "feature");
    RegDeleteValueA(hkey, "montecristo");
    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

    sprintf(keypath, udpath, usersid);
    res = RegOpenKeyA(HKEY_LOCAL_MACHINE, keypath, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(hkey, "feature", "VGtfp^p+,?82@JU1j_KE");
    CHECK_REG_STR(hkey, "montecristo", "VGtfp^p+,?82@JU1j_KE");

    RegDeleteValueA(hkey, "feature");
    RegDeleteValueA(hkey, "montecristo");
    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

    /* PublishFeatures, machine */
    r = MsiInstallProductA(msifile, "PUBLISH_FEATURES=1 ALLUSERS=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, featkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyA(HKEY_CURRENT_USER, cupath, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegOpenKeyA(HKEY_LOCAL_MACHINE, classfeat, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(hkey, "feature", "");
    CHECK_REG_STR(hkey, "montecristo", "");

    RegDeleteValueA(hkey, "feature");
    RegDeleteValueA(hkey, "montecristo");
    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

    sprintf(keypath, udpath, "S-1-5-18");
    res = RegOpenKeyA(HKEY_LOCAL_MACHINE, keypath, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(hkey, "feature", "VGtfp^p+,?82@JU1j_KE");
    CHECK_REG_STR(hkey, "montecristo", "VGtfp^p+,?82@JU1j_KE");

    RegDeleteValueA(hkey, "feature");
    RegDeleteValueA(hkey, "montecristo");
    RegDeleteKeyA(hkey, "");
    RegCloseKey(hkey);

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    HeapFree(GetProcessHeap(), 0, usersid);
}

static LPSTR reg_get_val_str(HKEY hkey, LPCSTR name)
{
    DWORD len = 0;
    LPSTR val;
    LONG r;

    r = RegQueryValueExA(hkey, name, NULL, NULL, NULL, &len);
    if (r != ERROR_SUCCESS)
        return NULL;

    len += sizeof (WCHAR);
    val = HeapAlloc(GetProcessHeap(), 0, len);
    if (!val) return NULL;
    val[0] = 0;
    RegQueryValueExA(hkey, name, NULL, NULL, (LPBYTE)val, &len);
    return val;
}

static void get_owner_company(LPSTR *owner, LPSTR *company)
{
    LONG res;
    HKEY hkey;
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    *owner = *company = NULL;

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    res = RegOpenKeyA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\MS Setup (ACME)\\User Info", &hkey);
    if (res == ERROR_SUCCESS)
    {
        *owner = reg_get_val_str(hkey, "DefName");
        *company = reg_get_val_str(hkey, "DefCompany");
        RegCloseKey(hkey);
    }

    if (!*owner || !*company)
    {
        res = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                            "Software\\Microsoft\\Windows\\CurrentVersion", 0, access, &hkey);
        if (res == ERROR_SUCCESS)
        {
            *owner = reg_get_val_str(hkey, "RegisteredOwner");
            *company = reg_get_val_str(hkey, "RegisteredOrganization");
            RegCloseKey(hkey);
        }
    }

    if (!*owner || !*company)
    {
        res = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                            "Software\\Microsoft\\Windows NT\\CurrentVersion", 0, access, &hkey);
        if (res == ERROR_SUCCESS)
        {
            *owner = reg_get_val_str(hkey, "RegisteredOwner");
            *company = reg_get_val_str(hkey, "RegisteredOrganization");
            RegCloseKey(hkey);
        }
    }
}

static void test_publish_registeruser(void)
{
    UINT r;
    LONG res;
    HKEY props;
    LPSTR usersid;
    LPSTR owner, company;
    CHAR keypath[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    static const CHAR keyfmt[] =
        "Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\"
        "UserData\\%s\\Products\\84A88FD7F6998CE40A22FB59F6B9C2BB\\InstallProperties";

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    if (!get_user_sid(&usersid))
        return;

    get_owner_company(&owner, &company);

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, pp_tables, sizeof(pp_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* RegisterUser, per-user */
    r = MsiInstallProductA(msifile, "REGISTER_USER=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    sprintf(keypath, keyfmt, usersid);
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &props);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(props, "ProductID", "none");
    CHECK_REG_STR(props, "RegCompany", company);
    CHECK_REG_STR(props, "RegOwner", owner);

    RegDeleteValueA(props, "ProductID");
    RegDeleteValueA(props, "RegCompany");
    RegDeleteValueA(props, "RegOwner");
    delete_key(props, "", access);
    RegCloseKey(props);

    /* RegisterUser, machine */
    r = MsiInstallProductA(msifile, "REGISTER_USER=1 ALLUSERS=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    sprintf(keypath, keyfmt, "S-1-5-18");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &props);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(props, "ProductID", "none");
    CHECK_REG_STR(props, "RegCompany", company);
    CHECK_REG_STR(props, "RegOwner", owner);

    RegDeleteValueA(props, "ProductID");
    RegDeleteValueA(props, "RegCompany");
    RegDeleteValueA(props, "RegOwner");
    delete_key(props, "", access);
    RegCloseKey(props);

error:
    HeapFree(GetProcessHeap(), 0, company);
    HeapFree(GetProcessHeap(), 0, owner);

    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    LocalFree(usersid);
}

static void test_publish_processcomponents(void)
{
    UINT r;
    LONG res;
    DWORD size;
    HKEY comp, hkey;
    LPSTR usersid;
    CHAR val[MAX_PATH];
    CHAR keypath[MAX_PATH];
    CHAR program_files_maximus[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    static const CHAR keyfmt[] =
        "Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\"
        "UserData\\%s\\Components\\%s";
    static const CHAR compkey[] =
        "Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\Components";

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    if (!get_user_sid(&usersid))
        return;

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, ppc_tables, sizeof(ppc_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* ProcessComponents, per-user */
    r = MsiInstallProductA(msifile, "PROCESS_COMPONENTS=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    sprintf(keypath, keyfmt, usersid, "CBABC2FDCCB35E749A8944D8C1C098B5");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &comp);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    size = MAX_PATH;
    res = RegQueryValueExA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB",
                           NULL, NULL, (LPBYTE)val, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    lstrcpyA(program_files_maximus,PROG_FILES_DIR);
    lstrcatA(program_files_maximus,"\\msitest\\maximus");

    ok(!lstrcmpiA(val, program_files_maximus),
       "Expected \"%s\", got \"%s\"\n", program_files_maximus, val);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, compkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    RegDeleteValueA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB");
    delete_key(comp, "", access);
    RegCloseKey(comp);

    sprintf(keypath, keyfmt, usersid, "241C3DA58FECD0945B9687D408766058");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &comp);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    size = MAX_PATH;
    res = RegQueryValueExA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB",
                           NULL, NULL, (LPBYTE)val, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(!lstrcmpA(val, "01\\msitest\\augustus"),
       "Expected \"01\\msitest\\augustus\", got \"%s\"\n", val);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, compkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    RegDeleteValueA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB");
    delete_key(comp, "", access);
    RegCloseKey(comp);

    /* ProcessComponents, machine */
    r = MsiInstallProductA(msifile, "PROCESS_COMPONENTS=1 ALLUSERS=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    sprintf(keypath, keyfmt, "S-1-5-18", "CBABC2FDCCB35E749A8944D8C1C098B5");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &comp);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    size = MAX_PATH;
    res = RegQueryValueExA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB",
                           NULL, NULL, (LPBYTE)val, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(!lstrcmpiA(val, program_files_maximus),
       "Expected \"%s\", got \"%s\"\n", program_files_maximus, val);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, compkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    RegDeleteValueA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB");
    delete_key(comp, "", access);
    RegCloseKey(comp);

    sprintf(keypath, keyfmt, "S-1-5-18", "241C3DA58FECD0945B9687D408766058");
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &comp);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    size = MAX_PATH;
    res = RegQueryValueExA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB",
                           NULL, NULL, (LPBYTE)val, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(!lstrcmpA(val, "01\\msitest\\augustus"),
       "Expected \"01\\msitest\\augustus\", got \"%s\"\n", val);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, compkey, 0, access, &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    RegDeleteValueA(comp, "84A88FD7F6998CE40A22FB59F6B9C2BB");
    delete_key(comp, "", access);
    RegCloseKey(comp);

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    LocalFree(usersid);
}

static void test_publish(void)
{
    UINT r;
    LONG res;
    HKEY uninstall, prodkey;
    INSTALLSTATE state;
    CHAR prodcode[] = "{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}";
    char date[MAX_PATH], temp[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    static const CHAR subkey[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    if (!pMsiQueryComponentStateA)
    {
        win_skip("MsiQueryComponentStateA is not available\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    get_date_str(date);
    GetTempPath(MAX_PATH, temp);

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, access, &uninstall);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, pp_tables, sizeof(pp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    /* nothing published */
    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    /* PublishProduct and RegisterProduct */
    r = MsiInstallProductA(msifile, "REGISTER_PRODUCT=1 PUBLISH_PRODUCT=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_DEFAULT, "Expected INSTALLSTATE_DEFAULT, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_COMPONENT, "Expected ERROR_UNKNOWN_COMPONENT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(prodkey, "DisplayName", "MSITEST");
    CHECK_REG_STR(prodkey, "DisplayVersion", "1.1.1");
    CHECK_REG_STR(prodkey, "InstallDate", date);
    CHECK_REG_STR(prodkey, "InstallSource", temp);
    CHECK_REG_ISTR(prodkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "Publisher", "Wine");
    CHECK_REG_STR(prodkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "AuthorizedCDFPrefix", NULL);
    CHECK_REG_STR(prodkey, "Comments", NULL);
    CHECK_REG_STR(prodkey, "Contact", NULL);
    CHECK_REG_STR(prodkey, "HelpLink", NULL);
    CHECK_REG_STR(prodkey, "HelpTelephone", NULL);
    CHECK_REG_STR(prodkey, "InstallLocation", NULL);
    CHECK_REG_STR(prodkey, "Readme", NULL);
    CHECK_REG_STR(prodkey, "Size", NULL);
    CHECK_REG_STR(prodkey, "URLInfoAbout", NULL);
    CHECK_REG_STR(prodkey, "URLUpdateInfo", NULL);
    CHECK_REG_DWORD(prodkey, "Language", 1033);
    CHECK_REG_DWORD(prodkey, "Version", 0x1010001);
    CHECK_REG_DWORD(prodkey, "VersionMajor", 1);
    CHECK_REG_DWORD(prodkey, "VersionMinor", 1);
    CHECK_REG_DWORD(prodkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_REG_DWORD2(prodkey, "EstimatedSize", 12, -12);
    }

    RegCloseKey(prodkey);

    r = MsiInstallProductA(msifile, "FULL=1 REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File deleted\n");
    ok(pf_exists("msitest"), "File deleted\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    /* complete install */
    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_DEFAULT, "Expected INSTALLSTATE_DEFAULT, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(prodkey, "DisplayName", "MSITEST");
    CHECK_REG_STR(prodkey, "DisplayVersion", "1.1.1");
    CHECK_REG_STR(prodkey, "InstallDate", date);
    CHECK_REG_STR(prodkey, "InstallSource", temp);
    CHECK_REG_ISTR(prodkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "Publisher", "Wine");
    CHECK_REG_STR(prodkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "AuthorizedCDFPrefix", NULL);
    CHECK_REG_STR(prodkey, "Comments", NULL);
    CHECK_REG_STR(prodkey, "Contact", NULL);
    CHECK_REG_STR(prodkey, "HelpLink", NULL);
    CHECK_REG_STR(prodkey, "HelpTelephone", NULL);
    CHECK_REG_STR(prodkey, "InstallLocation", NULL);
    CHECK_REG_STR(prodkey, "Readme", NULL);
    CHECK_REG_STR(prodkey, "Size", NULL);
    CHECK_REG_STR(prodkey, "URLInfoAbout", NULL);
    CHECK_REG_STR(prodkey, "URLUpdateInfo", NULL);
    CHECK_REG_DWORD(prodkey, "Language", 1033);
    CHECK_REG_DWORD(prodkey, "Version", 0x1010001);
    CHECK_REG_DWORD(prodkey, "VersionMajor", 1);
    CHECK_REG_DWORD(prodkey, "VersionMinor", 1);
    CHECK_REG_DWORD(prodkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_REG_DWORD2(prodkey, "EstimatedSize", 12, -12);
    }

    RegCloseKey(prodkey);

    /* no UnpublishFeatures */
    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!pf_exists("msitest\\maximus"), "File not deleted\n");
    ok(!pf_exists("msitest"), "Directory not deleted\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    /* complete install */
    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_DEFAULT, "Expected INSTALLSTATE_DEFAULT, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(prodkey, "DisplayName", "MSITEST");
    CHECK_REG_STR(prodkey, "DisplayVersion", "1.1.1");
    CHECK_REG_STR(prodkey, "InstallDate", date);
    CHECK_REG_STR(prodkey, "InstallSource", temp);
    CHECK_REG_ISTR(prodkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "Publisher", "Wine");
    CHECK_REG_STR(prodkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "AuthorizedCDFPrefix", NULL);
    CHECK_REG_STR(prodkey, "Comments", NULL);
    CHECK_REG_STR(prodkey, "Contact", NULL);
    CHECK_REG_STR(prodkey, "HelpLink", NULL);
    CHECK_REG_STR(prodkey, "HelpTelephone", NULL);
    CHECK_REG_STR(prodkey, "InstallLocation", NULL);
    CHECK_REG_STR(prodkey, "Readme", NULL);
    CHECK_REG_STR(prodkey, "Size", NULL);
    CHECK_REG_STR(prodkey, "URLInfoAbout", NULL);
    CHECK_REG_STR(prodkey, "URLUpdateInfo", NULL);
    CHECK_REG_DWORD(prodkey, "Language", 1033);
    CHECK_REG_DWORD(prodkey, "Version", 0x1010001);
    CHECK_REG_DWORD(prodkey, "VersionMajor", 1);
    CHECK_REG_DWORD(prodkey, "VersionMinor", 1);
    CHECK_REG_DWORD(prodkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_REG_DWORD2(prodkey, "EstimatedSize", 12, -12);
    }

    RegCloseKey(prodkey);

    /* UnpublishFeatures, only feature removed.  Only works when entire product is removed */
    r = MsiInstallProductA(msifile, "UNPUBLISH_FEATURES=1 REMOVE=feature");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File deleted\n");
    ok(pf_exists("msitest"), "Directory deleted\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_DEFAULT, "Expected INSTALLSTATE_DEFAULT, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(prodkey, "DisplayName", "MSITEST");
    CHECK_REG_STR(prodkey, "DisplayVersion", "1.1.1");
    CHECK_REG_STR(prodkey, "InstallDate", date);
    CHECK_REG_STR(prodkey, "InstallSource", temp);
    CHECK_REG_ISTR(prodkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "Publisher", "Wine");
    CHECK_REG_STR(prodkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "AuthorizedCDFPrefix", NULL);
    CHECK_REG_STR(prodkey, "Comments", NULL);
    CHECK_REG_STR(prodkey, "Contact", NULL);
    CHECK_REG_STR(prodkey, "HelpLink", NULL);
    CHECK_REG_STR(prodkey, "HelpTelephone", NULL);
    CHECK_REG_STR(prodkey, "InstallLocation", NULL);
    CHECK_REG_STR(prodkey, "Readme", NULL);
    CHECK_REG_STR(prodkey, "Size", NULL);
    CHECK_REG_STR(prodkey, "URLInfoAbout", NULL);
    CHECK_REG_STR(prodkey, "URLUpdateInfo", NULL);
    CHECK_REG_DWORD(prodkey, "Language", 1033);
    CHECK_REG_DWORD(prodkey, "Version", 0x1010001);
    CHECK_REG_DWORD(prodkey, "VersionMajor", 1);
    CHECK_REG_DWORD(prodkey, "VersionMinor", 1);
    CHECK_REG_DWORD(prodkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_REG_DWORD2(prodkey, "EstimatedSize", 12, -12);
    }

    RegCloseKey(prodkey);

    /* complete install */
    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_DEFAULT, "Expected INSTALLSTATE_DEFAULT, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(prodkey, "DisplayName", "MSITEST");
    CHECK_REG_STR(prodkey, "DisplayVersion", "1.1.1");
    CHECK_REG_STR(prodkey, "InstallDate", date);
    CHECK_REG_STR(prodkey, "InstallSource", temp);
    CHECK_REG_ISTR(prodkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "Publisher", "Wine");
    CHECK_REG_STR(prodkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "AuthorizedCDFPrefix", NULL);
    CHECK_REG_STR(prodkey, "Comments", NULL);
    CHECK_REG_STR(prodkey, "Contact", NULL);
    CHECK_REG_STR(prodkey, "HelpLink", NULL);
    CHECK_REG_STR(prodkey, "HelpTelephone", NULL);
    CHECK_REG_STR(prodkey, "InstallLocation", NULL);
    CHECK_REG_STR(prodkey, "Readme", NULL);
    CHECK_REG_STR(prodkey, "Size", NULL);
    CHECK_REG_STR(prodkey, "URLInfoAbout", NULL);
    CHECK_REG_STR(prodkey, "URLUpdateInfo", NULL);
    CHECK_REG_DWORD(prodkey, "Language", 1033);
    CHECK_REG_DWORD(prodkey, "Version", 0x1010001);
    CHECK_REG_DWORD(prodkey, "VersionMajor", 1);
    CHECK_REG_DWORD(prodkey, "VersionMinor", 1);
    CHECK_REG_DWORD(prodkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_REG_DWORD2(prodkey, "EstimatedSize", 12, -20);
    }

    RegCloseKey(prodkey);

    /* UnpublishFeatures, both features removed */
    r = MsiInstallProductA(msifile, "UNPUBLISH_FEATURES=1 REMOVE=feature,montecristo");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!pf_exists("msitest\\maximus"), "File not deleted\n");
    ok(!pf_exists("msitest"), "Directory not deleted\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    /* complete install */
    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_DEFAULT, "Expected INSTALLSTATE_DEFAULT, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(state == INSTALLSTATE_LOCAL, "Expected INSTALLSTATE_LOCAL, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    CHECK_REG_STR(prodkey, "DisplayName", "MSITEST");
    CHECK_REG_STR(prodkey, "DisplayVersion", "1.1.1");
    CHECK_REG_STR(prodkey, "InstallDate", date);
    CHECK_REG_STR(prodkey, "InstallSource", temp);
    CHECK_REG_ISTR(prodkey, "ModifyPath", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "Publisher", "Wine");
    CHECK_REG_STR(prodkey, "UninstallString", "MsiExec.exe /I{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    CHECK_REG_STR(prodkey, "AuthorizedCDFPrefix", NULL);
    CHECK_REG_STR(prodkey, "Comments", NULL);
    CHECK_REG_STR(prodkey, "Contact", NULL);
    CHECK_REG_STR(prodkey, "HelpLink", NULL);
    CHECK_REG_STR(prodkey, "HelpTelephone", NULL);
    CHECK_REG_STR(prodkey, "InstallLocation", NULL);
    CHECK_REG_STR(prodkey, "Readme", NULL);
    CHECK_REG_STR(prodkey, "Size", NULL);
    CHECK_REG_STR(prodkey, "URLInfoAbout", NULL);
    CHECK_REG_STR(prodkey, "URLUpdateInfo", NULL);
    CHECK_REG_DWORD(prodkey, "Language", 1033);
    CHECK_REG_DWORD(prodkey, "Version", 0x1010001);
    CHECK_REG_DWORD(prodkey, "VersionMajor", 1);
    CHECK_REG_DWORD(prodkey, "VersionMinor", 1);
    CHECK_REG_DWORD(prodkey, "WindowsInstaller", 1);
    todo_wine
    {
        CHECK_REG_DWORD2(prodkey, "EstimatedSize", 12, -12);
    }

    RegCloseKey(prodkey);

    /* complete uninstall */
    r = MsiInstallProductA(msifile, "FULL=1 REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!pf_exists("msitest\\maximus"), "File not deleted\n");
    ok(!pf_exists("msitest"), "Directory not deleted\n");

    state = MsiQueryProductState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "feature");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    state = MsiQueryFeatureState("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}", "montecristo");
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    r = pMsiQueryComponentStateA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                "{DF2CBABC-3BCC-47E5-A998-448D1C0C895B}", &state);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(state == INSTALLSTATE_UNKNOWN, "Expected INSTALLSTATE_UNKNOWN, got %d\n", state);

    res = RegOpenKeyExA(uninstall, prodcode, 0, access, &prodkey);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    /* make sure 'Program Files\msitest' is removed */
    delete_pfmsitest_files();

error:
    RegCloseKey(uninstall);
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
}

static void test_publishsourcelist(void)
{
    UINT r;
    DWORD size;
    CHAR value[MAX_PATH];
    CHAR path[MAX_PATH];
    CHAR prodcode[] = "{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}";

    if (!pMsiSourceListEnumSourcesA || !pMsiSourceListGetInfoA)
    {
        win_skip("MsiSourceListEnumSourcesA and/or MsiSourceListGetInfoA are not available\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);

    create_database(msifile, pp_tables, sizeof(pp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* nothing published */
    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_PACKAGENAME, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_URL, 0, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    r = MsiInstallProductA(msifile, "REGISTER_PRODUCT=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* after RegisterProduct */
    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_PACKAGENAME, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_URL, 0, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    r = MsiInstallProductA(msifile, "PROCESS_COMPONENTS=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* after ProcessComponents */
    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_PACKAGENAME, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_URL, 0, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    r = MsiInstallProductA(msifile, "PUBLISH_FEATURES=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* after PublishFeatures */
    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_PACKAGENAME, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_URL, 0, value, &size);
    ok(r == ERROR_UNKNOWN_PRODUCT, "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);
    ok(size == MAX_PATH, "Expected %d, got %d\n", MAX_PATH, size);
    ok(!lstrcmpA(value, "aaa"), "Expected \"aaa\", got \"%s\"\n", value);

    r = MsiInstallProductA(msifile, "PUBLISH_PRODUCT=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\maximus"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* after PublishProduct */
    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_PACKAGENAME, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!lstrcmpA(value, "msitest.msi"), "Expected 'msitest.msi', got %s\n", value);
    ok(size == 11, "Expected 11, got %d\n", size);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_MEDIAPACKAGEPATH, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!lstrcmpA(value, ""), "Expected \"\", got \"%s\"\n", value);
    ok(size == 0, "Expected 0, got %d\n", size);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_DISKPROMPT, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!lstrcmpA(value, ""), "Expected \"\", got \"%s\"\n", value);
    ok(size == 0, "Expected 0, got %d\n", size);

    lstrcpyA(path, CURR_DIR);
    lstrcatA(path, "\\");

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!lstrcmpA(value, path), "Expected \"%s\", got \"%s\"\n", path, value);
    ok(size == lstrlenA(path), "Expected %d, got %d\n", lstrlenA(path), size);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListGetInfoA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                               MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDTYPE, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!lstrcmpA(value, "n"), "Expected \"n\", got \"%s\"\n", value);
    ok(size == 1, "Expected 1, got %d\n", size);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_URL, 0, value, &size);
    ok(r == ERROR_NO_MORE_ITEMS, "Expected ERROR_NO_MORE_ITEMS, got %d\n", r);
    ok(!lstrcmpA(value, "aaa"), "Expected value to be unchanged, got %s\n", value);
    ok(size == MAX_PATH, "Expected MAX_PATH, got %d\n", size);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_NETWORK, 0, value, &size);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!lstrcmpA(value, path), "Expected \"%s\", got \"%s\"\n", path, value);
    ok(size == lstrlenA(path), "Expected %d, got %d\n", lstrlenA(path), size);

    size = MAX_PATH;
    lstrcpyA(value, "aaa");
    r = pMsiSourceListEnumSourcesA(prodcode, NULL, MSIINSTALLCONTEXT_USERUNMANAGED,
                                   MSICODE_PRODUCT | MSISOURCETYPE_NETWORK, 1, value, &size);
    ok(r == ERROR_NO_MORE_ITEMS, "Expected ERROR_NO_MORE_ITEMS, got %d\n", r);
    ok(!lstrcmpA(value, "aaa"), "Expected value to be unchanged, got %s\n", value);
    ok(size == MAX_PATH, "Expected MAX_PATH, got %d\n", size);

    /* complete uninstall */
    r = MsiInstallProductA(msifile, "FULL=1 REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!pf_exists("msitest\\maximus"), "File not deleted\n");
    ok(!pf_exists("msitest"), "Directory not deleted\n");

    /* make sure 'Program Files\msitest' is removed */
    delete_pfmsitest_files();

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
}

static UINT run_query(MSIHANDLE hdb, MSIHANDLE hrec, const char *query)
{
    MSIHANDLE hview = 0;
    UINT r;

    r = MsiDatabaseOpenView(hdb, query, &hview);
    if(r != ERROR_SUCCESS)
        return r;

    r = MsiViewExecute(hview, hrec);
    if(r == ERROR_SUCCESS)
        r = MsiViewClose(hview);
    MsiCloseHandle(hview);
    return r;
}

static void set_transform_summary_info(void)
{
    UINT r;
    MSIHANDLE suminfo = 0;

    /* build summary info */
    r = MsiGetSummaryInformation(0, mstfile, 3, &suminfo);
    ok(r == ERROR_SUCCESS , "Failed to open summaryinfo\n");

    r = MsiSummaryInfoSetProperty(suminfo, PID_TITLE, VT_LPSTR, 0, NULL, "MSITEST");
    ok(r == ERROR_SUCCESS, "Failed to set summary info\n");

    r = MsiSummaryInfoSetProperty(suminfo, PID_REVNUMBER, VT_LPSTR, 0, NULL,
                        "{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}1.1.1;"
                        "{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}1.1.1;"
                        "{4C0EAA15-0264-4E5A-8758-609EF142B92D}");
    ok(r == ERROR_SUCCESS , "Failed to set summary info\n");

    r = MsiSummaryInfoSetProperty(suminfo, PID_PAGECOUNT, VT_I4, 100, NULL, NULL);
    ok(r == ERROR_SUCCESS, "Failed to set summary info\n");

    r = MsiSummaryInfoPersist(suminfo);
    ok(r == ERROR_SUCCESS , "Failed to make summary info persist\n");

    r = MsiCloseHandle(suminfo);
    ok(r == ERROR_SUCCESS , "Failed to close suminfo\n");
}

static void generate_transform(void)
{
    MSIHANDLE hdb1, hdb2;
    LPCSTR query;
    UINT r;

    /* start with two identical databases */
    CopyFile(msifile, msifile2, FALSE);

    r = MsiOpenDatabase(msifile2, MSIDBOPEN_TRANSACT, &hdb1);
    ok(r == ERROR_SUCCESS , "Failed to create database\n");

    r = MsiDatabaseCommit(hdb1);
    ok(r == ERROR_SUCCESS , "Failed to commit database\n");

    r = MsiOpenDatabase(msifile, MSIDBOPEN_READONLY, &hdb2);
    ok(r == ERROR_SUCCESS , "Failed to create database\n");

    query = "INSERT INTO `Property` ( `Property`, `Value` ) VALUES ( 'prop', 'val' )";
    r = run_query(hdb1, 0, query);
    ok(r == ERROR_SUCCESS, "failed to add property\n");

    /* database needs to be committed */
    MsiDatabaseCommit(hdb1);

    r = MsiDatabaseGenerateTransform(hdb1, hdb2, mstfile, 0, 0);
    ok(r == ERROR_SUCCESS, "return code %d, should be ERROR_SUCCESS\n", r);

#if 0  /* not implemented in wine yet */
    r = MsiCreateTransformSummaryInfo(hdb2, hdb2, mstfile, 0, 0);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
#endif

    MsiCloseHandle(hdb1);
    MsiCloseHandle(hdb2);
}

/* data for generating a transform */

/* tables transform names - encoded as they would be in an msi database file */
static const WCHAR name1[] = { 0x4840, 0x3f3f, 0x4577, 0x446c, 0x3b6a, 0x45e4, 0x4824, 0 }; /* _StringData */
static const WCHAR name2[] = { 0x4840, 0x3f3f, 0x4577, 0x446c, 0x3e6a, 0x44b2, 0x482f, 0 }; /* _StringPool */
static const WCHAR name3[] = { 0x4840, 0x4559, 0x44f2, 0x4568, 0x4737, 0 }; /* Property */

/* data in each table */
static const char data1[] = /* _StringData */
    "propval";  /* all the strings squashed together */

static const WCHAR data2[] = { /* _StringPool */
/*  len, refs */
    0,   0,    /* string 0 ''     */
    4,   1,    /* string 1 'prop' */
    3,   1,    /* string 2 'val'  */
};

static const WCHAR data3[] = { /* Property */
    0x0201, 0x0001, 0x0002,
};

static const struct {
    LPCWSTR name;
    const void *data;
    DWORD size;
} table_transform_data[] =
{
    { name1, data1, sizeof data1 - 1 },
    { name2, data2, sizeof data2 },
    { name3, data3, sizeof data3 },
};

#define NUM_TRANSFORM_TABLES (sizeof table_transform_data/sizeof table_transform_data[0])

static void generate_transform_manual(void)
{
    IStorage *stg = NULL;
    IStream *stm;
    WCHAR name[0x20];
    HRESULT r;
    DWORD i, count;
    const DWORD mode = STGM_CREATE|STGM_READWRITE|STGM_DIRECT|STGM_SHARE_EXCLUSIVE;

    const CLSID CLSID_MsiTransform = { 0xc1082,0,0,{0xc0,0,0,0,0,0,0,0x46}};

    MultiByteToWideChar(CP_ACP, 0, mstfile, -1, name, 0x20);

    r = StgCreateDocfile(name, mode, 0, &stg);
    ok(r == S_OK, "failed to create storage\n");
    if (!stg)
        return;

    r = IStorage_SetClass(stg, &CLSID_MsiTransform);
    ok(r == S_OK, "failed to set storage type\n");

    for (i=0; i<NUM_TRANSFORM_TABLES; i++)
    {
        r = IStorage_CreateStream(stg, table_transform_data[i].name,
                            STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm);
        if (FAILED(r))
        {
            ok(0, "failed to create stream %08x\n", r);
            continue;
        }

        r = IStream_Write(stm, table_transform_data[i].data,
                          table_transform_data[i].size, &count);
        if (FAILED(r) || count != table_transform_data[i].size)
            ok(0, "failed to write stream\n");
        IStream_Release(stm);
    }

    IStorage_Release(stg);

    set_transform_summary_info();
}

static void test_transformprop(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, tp_tables, sizeof(tp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "File installed\n");

    if (0)
        generate_transform();
    else
        generate_transform_manual();

    r = MsiInstallProductA(msifile, "TRANSFORMS=winetest.mst");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    /* Delete the files in the temp (current) folder */
    DeleteFile(msifile);
    DeleteFile(msifile2);
    DeleteFile(mstfile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
}

static void test_currentworkingdir(void)
{
    UINT r;
    CHAR drive[MAX_PATH], path[MAX_PATH];
    LPSTR ptr;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, cwd_tables, sizeof(cwd_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    CreateDirectoryA("diffdir", NULL);
    SetCurrentDirectoryA("diffdir");

    sprintf(path, "..\\%s", msifile);
    r = MsiInstallProductA(path, NULL);
    todo_wine
    {
        ok(r == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %u\n", r);
        ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
        ok(!delete_pf("msitest", FALSE), "File installed\n");
    }

    sprintf(path, "%s\\%s", CURR_DIR, msifile);
    r = MsiInstallProductA(path, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    lstrcpyA(drive, CURR_DIR);
    drive[2] = '\\';
    drive[3] = '\0';
    SetCurrentDirectoryA(drive);

    lstrcpy(path, CURR_DIR);
    if (path[lstrlenA(path) - 1] != '\\')
        lstrcatA(path, "\\");
    lstrcatA(path, msifile);
    ptr = strchr(path, ':');
    ptr +=2;

    r = MsiInstallProductA(ptr, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    SetCurrentDirectoryA(CURR_DIR);
    DeleteFile(msifile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
    RemoveDirectory("diffdir");
}

static void set_admin_summary_info(const CHAR *name)
{
    MSIHANDLE db, summary;
    UINT r;

    r = MsiOpenDatabaseA(name, MSIDBOPEN_DIRECT, &db);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiGetSummaryInformationA(db, NULL, 1, &summary);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiSummaryInfoSetPropertyA(summary, PID_WORDCOUNT, VT_I4, 5, NULL, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    /* write the summary changes back to the stream */
    r = MsiSummaryInfoPersist(summary);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    MsiCloseHandle(summary);

    r = MsiDatabaseCommit(db);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    MsiCloseHandle(db);
}

static void test_admin(void)
{
    UINT r;

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, adm_tables, sizeof(adm_tables) / sizeof(msi_table));
    set_admin_summary_info(msifile);

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "File installed\n");
    ok(!DeleteFile("c:\\msitest\\augustus"), "File installed\n");
    ok(!RemoveDirectory("c:\\msitest"), "File installed\n");

    r = MsiInstallProductA(msifile, "ACTION=ADMIN");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "File installed\n");
    todo_wine
    {
        ok(DeleteFile("c:\\msitest\\augustus"), "File not installed\n");
        ok(RemoveDirectory("c:\\msitest"), "File not installed\n");
    }

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
}

static void set_admin_property_stream(LPCSTR file)
{
    IStorage *stg;
    IStream *stm;
    WCHAR fileW[MAX_PATH];
    HRESULT hr;
    DWORD count;
    const DWORD mode = STGM_DIRECT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE;

    /* AdminProperties */
    static const WCHAR stmname[] = {0x41ca,0x4330,0x3e71,0x44b5,0x4233,0x45f5,0x422c,0x4836,0};
    static const WCHAR data[] = {'M','Y','P','R','O','P','=','2','7','1','8',' ',
        'M','y','P','r','o','p','=','4','2',0};

    MultiByteToWideChar(CP_ACP, 0, file, -1, fileW, MAX_PATH);

    hr = StgOpenStorage(fileW, NULL, mode, NULL, 0, &stg);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    if (!stg)
        return;

    hr = IStorage_CreateStream(stg, stmname, STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    hr = IStream_Write(stm, data, sizeof(data), &count);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    IStream_Release(stm);
    IStorage_Release(stg);
}

static void test_adminprops(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, amp_tables, sizeof(amp_tables) / sizeof(msi_table));
    set_admin_summary_info(msifile);
    set_admin_property_stream(msifile);

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File installed\n");

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
}

static void create_pf_data(LPCSTR file, LPCSTR data, BOOL is_file)
{
    CHAR path[MAX_PATH];

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, file);

    if (is_file)
        create_file_data(path, data, 500);
    else
        CreateDirectoryA(path, NULL);
}

#define create_pf(file, is_file) create_pf_data(file, file, is_file)

static void test_removefiles(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\hydrogen", 500);
    create_file("msitest\\helium", 500);
    create_file("msitest\\lithium", 500);

    create_database(msifile, rem_tables, sizeof(rem_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(!pf_exists("msitest\\helium"), "File installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!pf_exists("msitest\\hydrogen"), "File not deleted\n");
    ok(!pf_exists("msitest\\helium"), "File not deleted\n");
    ok(delete_pf("msitest\\lithium", TRUE), "File deleted\n");
    ok(delete_pf("msitest", FALSE), "File deleted\n");

    create_pf("msitest", FALSE);
    create_pf("msitest\\hydrogen", TRUE);
    create_pf("msitest\\helium", TRUE);
    create_pf("msitest\\lithium", TRUE);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!pf_exists("msitest\\hydrogen"), "File not deleted\n");
    ok(delete_pf("msitest\\helium", TRUE), "File deleted\n");
    ok(delete_pf("msitest\\lithium", TRUE), "File deleted\n");
    ok(delete_pf("msitest", FALSE), "File deleted\n");

    create_pf("msitest", FALSE);
    create_pf("msitest\\furlong", TRUE);
    create_pf("msitest\\firkin", TRUE);
    create_pf("msitest\\fortnight", TRUE);
    create_pf("msitest\\becquerel", TRUE);
    create_pf("msitest\\dioptre", TRUE);
    create_pf("msitest\\attoparsec", TRUE);
    create_pf("msitest\\storeys", TRUE);
    create_pf("msitest\\block", TRUE);
    create_pf("msitest\\siriometer", TRUE);
    create_pf("msitest\\cabout", FALSE);
    create_pf("msitest\\cabout\\blocker", TRUE);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(!pf_exists("msitest\\helium"), "File installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(!pf_exists("msitest\\furlong"), "File not deleted\n");
    ok(!pf_exists("msitest\\firkin"), "File not deleted\n");
    ok(!pf_exists("msitest\\fortnight"), "File not deleted\n");
    ok(pf_exists("msitest\\becquerel"), "File not installed\n");
    ok(pf_exists("msitest\\dioptre"), "File not installed\n");
    ok(pf_exists("msitest\\attoparsec"), "File not installed\n");
    ok(!pf_exists("msitest\\storeys"), "File not deleted\n");
    ok(!pf_exists("msitest\\block"), "File not deleted\n");
    ok(!pf_exists("msitest\\siriometer"), "File not deleted\n");
    ok(pf_exists("msitest\\cabout"), "Directory removed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    create_pf("msitest\\furlong", TRUE);
    create_pf("msitest\\firkin", TRUE);
    create_pf("msitest\\fortnight", TRUE);
    create_pf("msitest\\storeys", TRUE);
    create_pf("msitest\\block", TRUE);
    create_pf("msitest\\siriometer", TRUE);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not deleted\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not deleted\n");
    ok(delete_pf("msitest\\lithium", TRUE), "File deleted\n");
    ok(delete_pf("msitest\\furlong", TRUE), "File deleted\n");
    ok(delete_pf("msitest\\firkin", TRUE), "File deleted\n");
    ok(delete_pf("msitest\\fortnight", TRUE), "File deleted\n");
    ok(!delete_pf("msitest\\becquerel", TRUE), "File not deleted\n");
    ok(!delete_pf("msitest\\dioptre", TRUE), "File not deleted\n");
    ok(delete_pf("msitest\\attoparsec", TRUE), "File deleted\n");
    ok(!delete_pf("msitest\\storeys", TRUE), "File not deleted\n");
    ok(!delete_pf("msitest\\block", TRUE), "File not deleted\n");
    ok(delete_pf("msitest\\siriometer", TRUE), "File deleted\n");
    ok(pf_exists("msitest\\cabout"), "Directory deleted\n");
    ok(pf_exists("msitest"), "Directory deleted\n");

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\hydrogen", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File installed\n");
    ok(delete_pf("msitest\\lithium", TRUE), "File not installed\n");
    ok(pf_exists("msitest\\cabout"), "Directory deleted\n");
    ok(pf_exists("msitest"), "Directory deleted\n");

    delete_pf("msitest\\cabout\\blocker", TRUE);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\cabout", FALSE), "Directory not deleted\n");
    ok(delete_pf("msitest", FALSE), "Directory deleted\n");

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\hydrogen");
    DeleteFile("msitest\\helium");
    DeleteFile("msitest\\lithium");
    RemoveDirectory("msitest");
}

static void test_movefiles(void)
{
    UINT r;
    char props[MAX_PATH];

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 100);
    create_file("cameroon", 100);
    create_file("djibouti", 100);
    create_file("egypt", 100);
    create_file("finland", 100);
    create_file("gambai", 100);
    create_file("honduras", 100);
    create_file("msitest\\india", 100);
    create_file("japan", 100);
    create_file("kenya", 100);
    CreateDirectoryA("latvia", NULL);
    create_file("nauru", 100);
    create_file("peru", 100);
    create_file("apple", 100);
    create_file("application", 100);
    create_file("ape", 100);
    create_file("foo", 100);
    create_file("fao", 100);
    create_file("fbod", 100);
    create_file("budding", 100);
    create_file("buddy", 100);
    create_file("bud", 100);
    create_file("bar", 100);
    create_file("bur", 100);
    create_file("bird", 100);

    create_database(msifile, mov_tables, sizeof(mov_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* if the source or dest property is not a full path,
     * windows tries to access it as a network resource
     */

    sprintf(props, "SOURCEFULL=\"%s\\\" DESTFULL=\"%s\\msitest\" "
            "FILEPATHBAD=\"%s\\japan\" FILEPATHGOOD=\"%s\\kenya\"",
            CURR_DIR, PROG_FILES_DIR, CURR_DIR, CURR_DIR);

    r = MsiInstallProductA(msifile, props);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\dest", TRUE), "File copied\n");
    ok(delete_pf("msitest\\canada", TRUE), "File not copied\n");
    ok(delete_pf("msitest\\dominica", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\elsalvador", TRUE), "File moved\n");
    ok(!delete_pf("msitest\\france", TRUE), "File moved\n");
    ok(!delete_pf("msitest\\georgia", TRUE), "File moved\n");
    ok(delete_pf("msitest\\hungary", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\indonesia", TRUE), "File moved\n");
    ok(!delete_pf("msitest\\jordan", TRUE), "File moved\n");
    ok(delete_pf("msitest\\kiribati", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\lebanon", TRUE), "File moved\n");
    ok(!delete_pf("msitest\\lebanon", FALSE), "Directory moved\n");
    ok(delete_pf("msitest\\poland", TRUE), "File not moved\n");
    /* either apple or application will be moved depending on directory order */
    if (!delete_pf("msitest\\apple", TRUE))
        ok(delete_pf("msitest\\application", TRUE), "File not moved\n");
    else
        ok(!delete_pf("msitest\\application", TRUE), "File should not exist\n");
    ok(delete_pf("msitest\\wildcard", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\ape", TRUE), "File moved\n");
    /* either fao or foo will be moved depending on directory order */
    if (delete_pf("msitest\\foo", TRUE))
        ok(!delete_pf("msitest\\fao", TRUE), "File should not exist\n");
    else
        ok(delete_pf("msitest\\fao", TRUE), "File not moved\n");
    ok(delete_pf("msitest\\single", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\fbod", TRUE), "File moved\n");
    ok(delete_pf("msitest\\budding", TRUE), "File not moved\n");
    ok(delete_pf("msitest\\buddy", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\bud", TRUE), "File moved\n");
    ok(delete_pf("msitest\\bar", TRUE), "File not moved\n");
    ok(delete_pf("msitest\\bur", TRUE), "File not moved\n");
    ok(!delete_pf("msitest\\bird", TRUE), "File moved\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");
    ok(DeleteFileA("cameroon"), "File moved\n");
    ok(!DeleteFileA("djibouti"), "File not moved\n");
    ok(DeleteFileA("egypt"), "File moved\n");
    ok(DeleteFileA("finland"), "File moved\n");
    ok(DeleteFileA("gambai"), "File moved\n");
    ok(!DeleteFileA("honduras"), "File not moved\n");
    ok(DeleteFileA("msitest\\india"), "File moved\n");
    ok(DeleteFileA("japan"), "File moved\n");
    ok(!DeleteFileA("kenya"), "File not moved\n");
    ok(RemoveDirectoryA("latvia"), "Directory moved\n");
    ok(!DeleteFileA("nauru"), "File not moved\n");
    ok(!DeleteFileA("peru"), "File not moved\n");
    ok(!DeleteFileA("apple"), "File not moved\n");
    ok(!DeleteFileA("application"), "File not moved\n");
    ok(DeleteFileA("ape"), "File moved\n");
    ok(!DeleteFileA("foo"), "File not moved\n");
    ok(!DeleteFileA("fao"), "File not moved\n");
    ok(DeleteFileA("fbod"), "File moved\n");
    ok(!DeleteFileA("budding"), "File not moved\n");
    ok(!DeleteFileA("buddy"), "File not moved\n");
    ok(DeleteFileA("bud"), "File moved\n");
    ok(!DeleteFileA("bar"), "File not moved\n");
    ok(!DeleteFileA("bur"), "File not moved\n");
    ok(DeleteFileA("bird"), "File moved\n");

error:
    DeleteFile("cameroon");
    DeleteFile("djibouti");
    DeleteFile("egypt");
    DeleteFile("finland");
    DeleteFile("gambai");
    DeleteFile("honduras");
    DeleteFile("japan");
    DeleteFile("kenya");
    DeleteFile("nauru");
    DeleteFile("peru");
    DeleteFile("apple");
    DeleteFile("application");
    DeleteFile("ape");
    DeleteFile("foo");
    DeleteFile("fao");
    DeleteFile("fbod");
    DeleteFile("budding");
    DeleteFile("buddy");
    DeleteFile("bud");
    DeleteFile("bar");
    DeleteFile("bur");
    DeleteFile("bird");
    DeleteFile("msitest\\india");
    DeleteFile("msitest\\augustus");
    RemoveDirectory("latvia");
    RemoveDirectory("msitest");
    DeleteFile(msifile);
}

static void test_missingcab(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);
    create_file("maximus", 500);

    create_database(msifile, mc_tables, sizeof(mc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    create_cab_file("test1.cab", MEDIA_SIZE, "maximus\0");

    create_pf("msitest", FALSE);
    create_pf_data("msitest\\caesar", "abcdefgh", TRUE);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS ||
       broken(r == ERROR_INSTALL_FAILURE), /* win9x */
       "Expected ERROR_SUCCESS, got %u\n", r);
    if (r == ERROR_SUCCESS)
    {
      ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
      ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    }
    ok(delete_pf("msitest\\caesar", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\gaius", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    create_pf("msitest", FALSE);
    create_pf_data("msitest\\caesar", "abcdefgh", TRUE);
    create_pf("msitest\\gaius", TRUE);

    r = MsiInstallProductA(msifile, "GAIUS=1");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    todo_wine
    {
        ok(!delete_pf("msitest\\maximus", TRUE), "File installed\n");
        ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    }
    ok(delete_pf("msitest\\caesar", TRUE), "File removed\n");
    ok(delete_pf("msitest\\gaius", TRUE), "File removed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    delete_pf("msitest\\caesar", TRUE);
    delete_pf("msitest", FALSE);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
    DeleteFile("maximus");
    DeleteFile("test1.cab");
    DeleteFile(msifile);
}

static void test_duplicatefiles(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);
    create_database(msifile, df_tables, sizeof(df_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* fails if the destination folder is not a valid property */

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\maximus", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\augustus", TRUE), "File not duplicated\n");
    ok(delete_pf("msitest\\this\\doesnot\\exist\\maximus", TRUE), "File not duplicated\n");
    ok(delete_pf("msitest\\this\\doesnot\\exist", FALSE), "File not duplicated\n");
    ok(delete_pf("msitest\\this\\doesnot", FALSE), "File not duplicated\n");
    ok(delete_pf("msitest\\this", FALSE), "File not duplicated\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    DeleteFile("msitest\\maximus");
    RemoveDirectory("msitest");
    DeleteFile(msifile);
}

static void test_writeregistryvalues(void)
{
    UINT r;
    LONG res;
    HKEY hkey;
    DWORD type, size;
    CHAR path[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, wrv_tables, sizeof(wrv_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File installed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", 0, access, &hkey);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    size = MAX_PATH;
    type = REG_MULTI_SZ;
    memset(path, 'a', MAX_PATH);
    res = RegQueryValueExA(hkey, "Value", NULL, &type, (LPBYTE)path, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(!memcmp(path, "one\0two\0three\0\0", size), "Wrong multi-sz data\n");
    ok(size == 15, "Expected 15, got %d\n", size);
    ok(type == REG_MULTI_SZ, "Expected REG_MULTI_SZ, got %d\n", type);

    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine\\msitest", access);
    delete_key(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wine", access);

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
}

static void test_sourcefolder(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("augustus", 500);

    create_database(msifile, sf_tables, sizeof(sf_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_INSTALL_FAILURE,
       "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    todo_wine
    {
        ok(!delete_pf("msitest", FALSE), "File installed\n");
    }

    RemoveDirectoryA("msitest");

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_INSTALL_FAILURE,
       "Expected ERROR_INSTALL_FAILURE, got %u\n", r);
    ok(!delete_pf("msitest\\augustus", TRUE), "File installed\n");
    todo_wine
    {
        ok(!delete_pf("msitest", FALSE), "File installed\n");
    }

error:
    DeleteFile(msifile);
    DeleteFile("augustus");
}

static void test_customaction51(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, ca51_tables, sizeof(ca51_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File installed\n");

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
}

static void test_installstate(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\alpha", 500);
    create_file("msitest\\beta", 500);
    create_file("msitest\\gamma", 500);
    create_file("msitest\\theta", 500);
    create_file("msitest\\delta", 500);
    create_file("msitest\\epsilon", 500);
    create_file("msitest\\zeta", 500);
    create_file("msitest\\iota", 500);
    create_file("msitest\\eta", 500);
    create_file("msitest\\kappa", 500);
    create_file("msitest\\lambda", 500);
    create_file("msitest\\mu", 500);

    create_database(msifile, is_tables, sizeof(is_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\alpha", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\beta", TRUE), "File installed\n");
    ok(delete_pf("msitest\\gamma", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\theta", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\delta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\epsilon", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\zeta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\iota", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\eta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\kappa", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\lambda", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\mu", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "ADDLOCAL=\"one,two,three,four\"");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\alpha", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\beta", TRUE), "File installed\n");
    ok(delete_pf("msitest\\gamma", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\theta", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\delta", TRUE), "File installed\n");
    ok(delete_pf("msitest\\epsilon", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\zeta", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\iota", TRUE), "File installed\n");
    ok(delete_pf("msitest\\eta", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\kappa", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\lambda", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\mu", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "ADDSOURCE=\"one,two,three,four\"");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\alpha", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\beta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\gamma", TRUE), "File installed\n");
    ok(delete_pf("msitest\\theta", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\delta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\epsilon", TRUE), "File installed\n");
    ok(delete_pf("msitest\\zeta", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\iota", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\eta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\kappa", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\lambda", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\mu", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=\"one,two,three,four\"");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\alpha", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\beta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\gamma", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\theta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\delta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\epsilon", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\zeta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\iota", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\eta", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\kappa", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\lambda", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\mu", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "File installed\n");

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\alpha");
    DeleteFile("msitest\\beta");
    DeleteFile("msitest\\gamma");
    DeleteFile("msitest\\theta");
    DeleteFile("msitest\\delta");
    DeleteFile("msitest\\epsilon");
    DeleteFile("msitest\\zeta");
    DeleteFile("msitest\\iota");
    DeleteFile("msitest\\eta");
    DeleteFile("msitest\\kappa");
    DeleteFile("msitest\\lambda");
    DeleteFile("msitest\\mu");
    RemoveDirectory("msitest");
}

struct sourcepathmap
{
    BOOL sost; /* shortone\shorttwo */
    BOOL solt; /* shortone\longtwo */
    BOOL lost; /* longone\shorttwo */
    BOOL lolt; /* longone\longtwo */
    BOOL soste; /* shortone\shorttwo source exists */
    BOOL solte; /* shortone\longtwo source exists */
    BOOL loste; /* longone\shorttwo source exists */
    BOOL lolte; /* longone\longtwo source exists */
    UINT err;
    DWORD size;
} spmap[256] =
{
    {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, ERROR_SUCCESS, 200},
    {FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, ERROR_INSTALL_FAILURE, 0},
    {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, ERROR_INSTALL_FAILURE, 0},
};

static DWORD get_pf_file_size(LPCSTR file)
{
    CHAR path[MAX_PATH];
    HANDLE hfile;
    DWORD size;

    lstrcpyA(path, PROG_FILES_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, file);

    hfile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hfile == INVALID_HANDLE_VALUE)
        return INVALID_FILE_SIZE;

    size = GetFileSize(hfile, NULL);
    CloseHandle(hfile);
    return size;
}

static void test_sourcepath(void)
{
    UINT r, i;

    if (!winetest_interactive)
    {
        skip("Run in interactive mode to run source path tests.\n");
        return;
    }

    create_database(msifile, sp_tables, sizeof(sp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    for (i = 0; i < sizeof(spmap) / sizeof(spmap[0]); i++)
    {
        if (spmap[i].sost)
        {
            CreateDirectoryA("shortone", NULL);
            CreateDirectoryA("shortone\\shorttwo", NULL);
        }

        if (spmap[i].solt)
        {
            CreateDirectoryA("shortone", NULL);
            CreateDirectoryA("shortone\\longtwo", NULL);
        }

        if (spmap[i].lost)
        {
            CreateDirectoryA("longone", NULL);
            CreateDirectoryA("longone\\shorttwo", NULL);
        }

        if (spmap[i].lolt)
        {
            CreateDirectoryA("longone", NULL);
            CreateDirectoryA("longone\\longtwo", NULL);
        }

        if (spmap[i].soste)
            create_file("shortone\\shorttwo\\augustus", 50);
        if (spmap[i].solte)
            create_file("shortone\\longtwo\\augustus", 100);
        if (spmap[i].loste)
            create_file("longone\\shorttwo\\augustus", 150);
        if (spmap[i].lolte)
            create_file("longone\\longtwo\\augustus", 200);

        r = MsiInstallProductA(msifile, NULL);
        ok(r == spmap[i].err, "%d: Expected %d, got %d\n", i, spmap[i].err, r);
        ok(get_pf_file_size("msitest\\augustus") == spmap[i].size,
           "%d: Expected %d, got %d\n", i, spmap[i].size,
           get_pf_file_size("msitest\\augustus"));

        if (r == ERROR_SUCCESS)
        {
            ok(delete_pf("msitest\\augustus", TRUE), "%d: File not installed\n", i);
            ok(delete_pf("msitest", FALSE), "%d: File not installed\n", i);
        }
        else
        {
            ok(!delete_pf("msitest\\augustus", TRUE), "%d: File installed\n", i);
            todo_wine ok(!delete_pf("msitest", FALSE), "%d: File installed\n", i);
        }

        DeleteFileA("shortone\\shorttwo\\augustus");
        DeleteFileA("shortone\\longtwo\\augustus");
        DeleteFileA("longone\\shorttwo\\augustus");
        DeleteFileA("longone\\longtwo\\augustus");
        RemoveDirectoryA("shortone\\shorttwo");
        RemoveDirectoryA("shortone\\longtwo");
        RemoveDirectoryA("longone\\shorttwo");
        RemoveDirectoryA("longone\\longtwo");
        RemoveDirectoryA("shortone");
        RemoveDirectoryA("longone");
    }

    DeleteFileA(msifile);
}

static void test_MsiConfigureProductEx(void)
{
    UINT r;
    LONG res;
    DWORD type, size;
    HKEY props, source;
    CHAR keypath[MAX_PATH * 2], localpack[MAX_PATH];
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    if (on_win9x)
    {
        win_skip("Different registry keys on Win9x and WinMe\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\hydrogen", 500);
    create_file("msitest\\helium", 500);
    create_file("msitest\\lithium", 500);

    create_database(msifile, mcp_tables, sizeof(mcp_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* NULL szProduct */
    r = MsiConfigureProductExA(NULL, INSTALLLEVEL_DEFAULT,
                               INSTALLSTATE_DEFAULT, "PROPVAR=42");
    ok(r == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n", r);

    /* empty szProduct */
    r = MsiConfigureProductExA("", INSTALLLEVEL_DEFAULT,
                               INSTALLSTATE_DEFAULT, "PROPVAR=42");
    ok(r == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n", r);

    /* garbage szProduct */
    r = MsiConfigureProductExA("garbage", INSTALLLEVEL_DEFAULT,
                               INSTALLSTATE_DEFAULT, "PROPVAR=42");
    ok(r == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n", r);

    /* guid without brackets */
    r = MsiConfigureProductExA("6700E8CF-95AB-4D9C-BC2C-15840DEA7A5D",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT,
                               "PROPVAR=42");
    ok(r == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n", r);

    /* guid with brackets */
    r = MsiConfigureProductExA("{6700E8CF-95AB-4D9C-BC2C-15840DEA7A5D}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT,
                               "PROPVAR=42");
    ok(r == ERROR_UNKNOWN_PRODUCT,
       "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);

    /* same length as guid, but random */
    r = MsiConfigureProductExA("A938G02JF-2NF3N93-VN3-2NNF-3KGKALDNF93",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT,
                               "PROPVAR=42");
    ok(r == ERROR_UNKNOWN_PRODUCT,
       "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);

    /* product not installed yet */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT,
                               "PROPVAR=42");
    ok(r == ERROR_UNKNOWN_PRODUCT,
       "Expected ERROR_UNKNOWN_PRODUCT, got %d\n", r);

    /* install the product, per-user unmanaged */
    r = MsiInstallProductA(msifile, "INSTALLLEVEL=10 PROPVAR=42");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* product is installed per-user managed, remove it */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\lithium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest", FALSE), "Directory not removed\n");

    /* product has been removed */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT,
                               "PROPVAR=42");
    ok(r == ERROR_UNKNOWN_PRODUCT,
       "Expected ERROR_UNKNOWN_PRODUCT, got %u\n", r);

    /* install the product, machine */
    r = MsiInstallProductA(msifile, "ALLUSERS=1 INSTALLLEVEL=10 PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* product is installed machine, remove it */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\lithium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest", FALSE), "Directory not removed\n");

    /* product has been removed */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT,
                               "PROPVAR=42");
    ok(r == ERROR_UNKNOWN_PRODUCT,
       "Expected ERROR_UNKNOWN_PRODUCT, got %u\n", r);

    /* install the product, machine */
    r = MsiInstallProductA(msifile, "ALLUSERS=1 INSTALLLEVEL=10 PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    DeleteFileA(msifile);

    /* local msifile is removed */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\lithium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest", FALSE), "Directory not removed\n");

    create_database(msifile, mcp_tables, sizeof(mcp_tables) / sizeof(msi_table));

    /* install the product, machine */
    r = MsiInstallProductA(msifile, "ALLUSERS=1 INSTALLLEVEL=10 PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    DeleteFileA(msifile);

    lstrcpyA(keypath, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\");
    lstrcatA(keypath, "Installer\\UserData\\S-1-5-18\\Products\\");
    lstrcatA(keypath, "84A88FD7F6998CE40A22FB59F6B9C2BB\\InstallProperties");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &props);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegSetValueExA(props, "LocalPackage", 0, REG_SZ,
                         (const BYTE *)"C:\\idontexist.msi", 18);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    /* LocalPackage is used to find the cached msi package */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_INSTALL_SOURCE_ABSENT,
       "Expected ERROR_INSTALL_SOURCE_ABSENT, got %d\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    RegCloseKey(props);
    create_database(msifile, mcp_tables, sizeof(mcp_tables) / sizeof(msi_table));

    /* LastUsedSource (local msi package) can be used as a last resort */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\lithium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest", FALSE), "Directory not removed\n");

    /* install the product, machine */
    r = MsiInstallProductA(msifile, "ALLUSERS=1 INSTALLLEVEL=10 PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    lstrcpyA(keypath, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\");
    lstrcatA(keypath, "Installer\\UserData\\S-1-5-18\\Products\\");
    lstrcatA(keypath, "84A88FD7F6998CE40A22FB59F6B9C2BB\\InstallProperties");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &props);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegSetValueExA(props, "LocalPackage", 0, REG_SZ,
                         (const BYTE *)"C:\\idontexist.msi", 18);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    lstrcpyA(keypath, "SOFTWARE\\Classes\\Installer\\Products\\");
    lstrcatA(keypath, "84A88FD7F6998CE40A22FB59F6B9C2BB\\SourceList");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keypath, 0, access, &source);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    type = REG_SZ;
    size = MAX_PATH;
    res = RegQueryValueExA(source, "PackageName", NULL, &type,
                           (LPBYTE)localpack, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegSetValueExA(source, "PackageName", 0, REG_SZ,
                         (const BYTE *)"idontexist.msi", 15);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    /* SourceList is altered */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_INSTALL_SOURCE_ABSENT,
       "Expected ERROR_INSTALL_SOURCE_ABSENT, got %d\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    /* restore the SourceList */
    res = RegSetValueExA(source, "PackageName", 0, REG_SZ,
                         (const BYTE *)localpack, lstrlenA(localpack) + 1);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    /* finally remove the product */
    r = MsiConfigureProductExA("{7DF88A48-996F-4EC8-A022-BF956F9B2CBB}",
                               INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT,
                               "PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\lithium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest", FALSE), "File not removed\n");

    RegCloseKey(source);
    RegCloseKey(props);

error:
    DeleteFileA("msitest\\hydrogen");
    DeleteFileA("msitest\\helium");
    DeleteFileA("msitest\\lithium");
    RemoveDirectoryA("msitest");
    DeleteFileA(msifile);
}

static void test_missingcomponent(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\hydrogen", 500);
    create_file("msitest\\helium", 500);
    create_file("msitest\\lithium", 500);
    create_file("beryllium", 500);

    create_database(msifile, mcomp_tables, sizeof(mcomp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, "INSTALLLEVEL=10 PROPVAR=42");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(pf_exists("msitest\\hydrogen"), "File not installed\n");
    ok(pf_exists("msitest\\helium"), "File not installed\n");
    ok(pf_exists("msitest\\lithium"), "File not installed\n");
    ok(!pf_exists("msitest\\beryllium"), "File installed\n");
    ok(pf_exists("msitest"), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL INSTALLLEVEL=10 PROPVAR=42");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(!delete_pf("msitest\\hydrogen", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\helium", TRUE), "File not removed\n");
    ok(!delete_pf("msitest\\lithium", TRUE), "File not removed\n");
    ok(!pf_exists("msitest\\beryllium"), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "Directory not removed\n");

error:
    DeleteFileA(msifile);
    DeleteFileA("msitest\\hydrogen");
    DeleteFileA("msitest\\helium");
    DeleteFileA("msitest\\lithium");
    DeleteFileA("beryllium");
    RemoveDirectoryA("msitest");
}

static void test_sourcedirprop(void)
{
    UINT r;
    CHAR props[MAX_PATH];

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, ca51_tables, sizeof(ca51_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File installed\n");

    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");

    CreateDirectoryA("altsource", NULL);
    CreateDirectoryA("altsource\\msitest", NULL);
    create_file("altsource\\msitest\\augustus", 500);

    sprintf(props, "SRCDIR=%s\\altsource\\", CURR_DIR);

    r = MsiInstallProductA(msifile, props);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File installed\n");

    DeleteFile("altsource\\msitest\\augustus");
    RemoveDirectory("altsource\\msitest");
    RemoveDirectory("altsource");

error:
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
    DeleteFile(msifile);
}

static void test_adminimage(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    CreateDirectoryA("msitest\\first", NULL);
    CreateDirectoryA("msitest\\second", NULL);
    CreateDirectoryA("msitest\\cabout", NULL);
    CreateDirectoryA("msitest\\cabout\\new", NULL);
    create_file("msitest\\one.txt", 100);
    create_file("msitest\\first\\two.txt", 100);
    create_file("msitest\\second\\three.txt", 100);
    create_file("msitest\\cabout\\four.txt", 100);
    create_file("msitest\\cabout\\new\\five.txt", 100);
    create_file("msitest\\filename", 100);
    create_file("msitest\\service.exe", 100);

    create_database_wordcount(msifile, ai_tables,
                              sizeof(ai_tables) / sizeof(msi_table),
                              msidbSumInfoSourceTypeAdminImage);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    DeleteFileA("msifile");
    DeleteFileA("msitest\\cabout\\new\\five.txt");
    DeleteFileA("msitest\\cabout\\four.txt");
    DeleteFileA("msitest\\second\\three.txt");
    DeleteFileA("msitest\\first\\two.txt");
    DeleteFileA("msitest\\one.txt");
    DeleteFileA("msitest\\service.exe");
    DeleteFileA("msitest\\filename");
    RemoveDirectoryA("msitest\\cabout\\new");
    RemoveDirectoryA("msitest\\cabout");
    RemoveDirectoryA("msitest\\second");
    RemoveDirectoryA("msitest\\first");
    RemoveDirectoryA("msitest");
}

static void test_propcase(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\augustus", 500);

    create_database(msifile, pc_tables, sizeof(pc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, "MyProp=42");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
    ok(delete_pf("msitest\\augustus", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    DeleteFile(msifile);
    DeleteFile("msitest\\augustus");
    RemoveDirectory("msitest");
}

static void test_int_widths( void )
{
    static const char int0[] = "int0\ni0\nint0\tint0\n1";
    static const char int1[] = "int1\ni1\nint1\tint1\n1";
    static const char int2[] = "int2\ni2\nint2\tint2\n1";
    static const char int3[] = "int3\ni3\nint3\tint3\n1";
    static const char int4[] = "int4\ni4\nint4\tint4\n1";
    static const char int5[] = "int5\ni5\nint5\tint5\n1";
    static const char int8[] = "int8\ni8\nint8\tint8\n1";

    static const struct
    {
        const char  *data;
        unsigned int size;
        UINT         ret;
    }
    tests[] =
    {
        { int0, sizeof(int0) - 1, ERROR_SUCCESS },
        { int1, sizeof(int1) - 1, ERROR_SUCCESS },
        { int2, sizeof(int2) - 1, ERROR_SUCCESS },
        { int3, sizeof(int3) - 1, ERROR_FUNCTION_FAILED },
        { int4, sizeof(int4) - 1, ERROR_SUCCESS },
        { int5, sizeof(int5) - 1, ERROR_FUNCTION_FAILED },
        { int8, sizeof(int8) - 1, ERROR_FUNCTION_FAILED }
    };

    char tmpdir[MAX_PATH], msitable[MAX_PATH], msidb[MAX_PATH];
    MSIHANDLE db;
    UINT r, i;

    GetTempPathA(MAX_PATH, tmpdir);
    CreateDirectoryA(tmpdir, NULL);

    strcpy(msitable, tmpdir);
    strcat(msitable, "\\msitable.idt");

    strcpy(msidb, tmpdir);
    strcat(msidb, "\\msitest.msi");

    r = MsiOpenDatabaseA(msidb, MSIDBOPEN_CREATE, &db);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        write_file(msitable, tests[i].data, tests[i].size);

        r = MsiDatabaseImportA(db, tmpdir, "msitable.idt");
        ok(r == tests[i].ret, " %u expected %u, got %u\n", i, tests[i].ret, r);

        r = MsiDatabaseCommit(db);
        ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);
        DeleteFileA(msitable);
    }

    MsiCloseHandle(db);
    DeleteFileA(msidb);
    RemoveDirectoryA(tmpdir);
}

static void test_shortcut(void)
{
    UINT r;
    HRESULT hr;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, sc_tables, sizeof(sc_tables) / sizeof(msi_table));

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ok(SUCCEEDED(hr), "CoInitialize failed 0x%08x\n", hr);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    CoUninitialize();

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ok(SUCCEEDED(hr), "CoInitialize failed 0x%08x\n", hr);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    CoUninitialize();

    delete_pf("msitest\\cabout\\new\\five.txt", TRUE);
    delete_pf("msitest\\cabout\\new", FALSE);
    delete_pf("msitest\\cabout\\four.txt", TRUE);
    delete_pf("msitest\\cabout", FALSE);
    delete_pf("msitest\\changed\\three.txt", TRUE);
    delete_pf("msitest\\changed", FALSE);
    delete_pf("msitest\\first\\two.txt", TRUE);
    delete_pf("msitest\\first", FALSE);
    delete_pf("msitest\\filename", TRUE);
    delete_pf("msitest\\one.txt", TRUE);
    delete_pf("msitest\\service.exe", TRUE);
    delete_pf("msitest\\Shortcut.lnk", TRUE);
    delete_pf("msitest", FALSE);

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_envvar(void)
{
    UINT r;
    HKEY env;
    LONG res;
    DWORD type, size;
    char buffer[16];
    UINT i;

    if (on_win9x)
    {
        win_skip("Environment variables are handled differently on Win9x and WinMe\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, env_tables, sizeof(env_tables) / sizeof(msi_table));

    res = RegCreateKeyExA(HKEY_CURRENT_USER, "Environment", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &env, NULL);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegSetValueExA(env, "MSITESTVAR1", 0, REG_SZ, (const BYTE *)"0", 2);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegSetValueExA(env, "MSITESTVAR2", 0, REG_SZ, (const BYTE *)"0", 2);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    type = REG_NONE;
    size = sizeof(buffer);
    buffer[0] = 0;
    res = RegQueryValueExA(env, "MSITESTVAR1", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "Expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "Expected \"1\", got %s\n", buffer);

    res = RegDeleteValueA(env, "MSITESTVAR1");
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    type = REG_NONE;
    size = sizeof(buffer);
    buffer[0] = 0;
    res = RegQueryValueExA(env, "MSITESTVAR2", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "Expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "Expected \"1\", got %s\n", buffer);

    res = RegDeleteValueA(env, "MSITESTVAR2");
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR3");
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR4");
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR5");
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR6");
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR7");
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR8");
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR9");
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegDeleteValueA(env, "MSITESTVAR10");
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    i = 11;
    while (environment_dat_results[(i-11)]) {
        char name[20];
        sprintf(name, "MSITESTVAR%d", i);

        type = REG_NONE;
        size = sizeof(buffer);
        buffer[0] = 0;
        res = RegQueryValueExA(env, name, NULL, &type, (LPBYTE)buffer, &size);
        ok(res == ERROR_SUCCESS, "%d: Expected ERROR_SUCCESS, got %d\n", i, res);
        ok(type == REG_SZ, "%d: Expected REG_SZ, got %u\n", i, type);
        ok(!lstrcmp(buffer, environment_dat_results[(i-11)]), "%d: Expected %s, got %s\n",
           i, environment_dat_results[(i-11)], buffer);

        res = RegDeleteValueA(env, name);
        ok(res == ERROR_SUCCESS, "%d: Expected ERROR_SUCCESS, got %d\n", i, res);
        i++;
    }

    delete_pf("msitest\\cabout\\new\\five.txt", TRUE);
    delete_pf("msitest\\cabout\\new", FALSE);
    delete_pf("msitest\\cabout\\four.txt", TRUE);
    delete_pf("msitest\\cabout", FALSE);
    delete_pf("msitest\\changed\\three.txt", TRUE);
    delete_pf("msitest\\changed", FALSE);
    delete_pf("msitest\\first\\two.txt", TRUE);
    delete_pf("msitest\\first", FALSE);
    delete_pf("msitest\\filename", TRUE);
    delete_pf("msitest\\one.txt", TRUE);
    delete_pf("msitest\\service.exe", TRUE);
    delete_pf("msitest", FALSE);

error:
    RegDeleteValueA(env, "MSITESTVAR1");
    RegDeleteValueA(env, "MSITESTVAR2");
    RegCloseKey(env);

    delete_test_files();
    DeleteFile(msifile);
}

static void test_preselected(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, ps_tables, sizeof(ps_tables) / sizeof(msi_table));

    r = MsiInstallProductA(msifile, "ADDLOCAL=One");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout\\new", FALSE), "File installed\n");
    ok(!delete_pf("msitest\\cabout\\four.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout", FALSE), "File installed\n");
    ok(!delete_pf("msitest\\changed\\three.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\changed", FALSE), "File installed\n");
    ok(!delete_pf("msitest\\first\\two.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\first", FALSE), "File installed\n");
    ok(!delete_pf("msitest\\filename", TRUE), "File installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\service.exe", TRUE), "File installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(!delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_installed_prop(void)
{
    static char prodcode[] = "{7df88a48-996f-4ec8-a022-bf956f9b2cbb}";
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, ip_tables, sizeof(ip_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, "FULL=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    r = MsiConfigureProductExA(prodcode, INSTALLLEVEL_DEFAULT, INSTALLSTATE_DEFAULT, "FULL=1");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_allusers_prop(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, aup_tables, sizeof(aup_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* ALLUSERS property unset */
    r = MsiInstallProductA(msifile, "FULL=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    delete_test_files();

    create_test_files();
    create_database(msifile, aup2_tables, sizeof(aup2_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* ALLUSERS property set to 1 */
    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    delete_test_files();

    create_test_files();
    create_database(msifile, aup3_tables, sizeof(aup3_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* ALLUSERS property set to 2 */
    r = MsiInstallProductA(msifile, "FULL=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "File not installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    delete_test_files();

    create_test_files();
    create_database(msifile, aup4_tables, sizeof(aup4_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* ALLUSERS property set to 2, conditioned on ALLUSERS = 1 */
    r = MsiInstallProductA(msifile, "FULL=1");
    if (r == ERROR_SUCCESS)
    {
        /* Win9x/WinMe */
        win_skip("Win9x and WinMe act differently with respect to ALLUSERS\n");

        ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\cabout\\new", FALSE), "File not installed\n");
        ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\cabout", FALSE), "File not installed\n");
        ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\changed", FALSE), "File not installed\n");
        ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\first", FALSE), "File not installed\n");
        ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
        ok(delete_pf("msitest\\one.txt", TRUE), "File installed\n");
        ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
        ok(delete_pf("msitest", FALSE), "File not installed\n");

        r = MsiInstallProductA(msifile, "REMOVE=ALL");
        ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

        delete_test_files();
    }
    else
        ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

error:
    delete_test_files();
    DeleteFile(msifile);
}

static char session_manager[] = "System\\CurrentControlSet\\Control\\Session Manager";
static char rename_ops[]      = "PendingFileRenameOperations";

static void process_pending_renames(HKEY hkey)
{
    char *buf, *src, *dst, *buf2, *buf2ptr;
    DWORD size, buf2len = 0;
    LONG ret;
    BOOL found = FALSE;

    ret = RegQueryValueExA(hkey, rename_ops, NULL, NULL, NULL, &size);
    buf = HeapAlloc(GetProcessHeap(), 0, size);
    buf2ptr = buf2 = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    buf[0] = 0;

    ret = RegQueryValueExA(hkey, rename_ops, NULL, NULL, (LPBYTE)buf, &size);
    ok(!ret, "RegQueryValueExA failed %d (%u)\n", ret, GetLastError());

    for (src = buf; *src; src = dst + strlen(dst) + 1)
    {
        DWORD flags = MOVEFILE_COPY_ALLOWED;

        dst = src + strlen(src) + 1;

        if (!strstr(src, "msitest"))
        {
            lstrcpyA(buf2ptr, src);
            buf2len += strlen(src) + 1;
            buf2ptr += strlen(src) + 1;
            if (*dst)
            {
                lstrcpyA(buf2ptr, dst);
                buf2ptr += strlen(dst) + 1;
                buf2len += strlen(dst) + 1;
            }
            buf2ptr++;
            buf2len++;
            continue;
        }

        found = TRUE;

        if (*dst == '!')
        {
            flags |= MOVEFILE_REPLACE_EXISTING;
            dst++;
        }
        if (src[0] == '\\' && src[1] == '?' && src[2] == '?' && src[3] == '\\') src += 4;
        if (*dst)
        {
            if (dst[0] == '\\' && dst[1] == '?' && dst[2] == '?' && dst[3] == '\\') dst += 4;
            ok(MoveFileExA(src, dst, flags), "Failed to move file %s -> %s (%u)\n", src, dst, GetLastError());
        }
        else
            ok(DeleteFileA(src), "Failed to delete file %s (%u)\n", src, GetLastError());
    }

    ok(found, "Expected a 'msitest' entry\n");

    if (*buf2)
    {
        buf2len++;
        RegSetValueExA(hkey, rename_ops, 0, REG_MULTI_SZ, (LPBYTE)buf2, buf2len);
    }
    else
        RegDeleteValueA(hkey, rename_ops);

    HeapFree(GetProcessHeap(), 0, buf);
    HeapFree(GetProcessHeap(), 0, buf2);
}

static BOOL file_matches_data(LPCSTR file, LPCSTR data)
{
    DWORD len, data_len = strlen(data);
    HANDLE handle;
    char buf[128];

    handle = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ok(handle != INVALID_HANDLE_VALUE, "failed to open %s (%u)\n", file, GetLastError());

    if (ReadFile(handle, buf, sizeof(buf), &len, NULL) && len >= data_len)
    {
        CloseHandle(handle);
        return !memcmp(buf, data, data_len);
    }
    CloseHandle(handle);
    return FALSE;
}

static void test_file_in_use(void)
{
    UINT r;
    HANDLE file;
    HKEY hkey;
    char path[MAX_PATH];

    if (on_win9x)
    {
        win_skip("Pending file renaming is implemented differently on Win9x and WinMe\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    RegOpenKeyExA(HKEY_LOCAL_MACHINE, session_manager, 0, KEY_ALL_ACCESS, &hkey);

    CreateDirectoryA("msitest", NULL);
    create_file("msitest\\maximus", 500);
    create_database(msifile, fiu_tables, sizeof(fiu_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    lstrcpy(path, PROG_FILES_DIR);
    lstrcat(path, "\\msitest");
    CreateDirectoryA(path, NULL);

    lstrcat(path, "\\maximus");
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    r = MsiInstallProductA(msifile, "REBOOT=ReallySuppress FULL=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS_REBOOT_REQUIRED, "Expected ERROR_SUCCESS_REBOOT_REQUIRED got %u\n", r);
    ok(!file_matches_data(path, "msitest\\maximus"), "Expected file not to match\n");
    CloseHandle(file);
    ok(!file_matches_data(path, "msitest\\maximus"), "Expected file not to match\n");

    process_pending_renames(hkey);
    RegCloseKey(hkey);

    ok(file_matches_data(path, "msitest\\maximus"), "Expected file to match\n");
    ok(delete_pf("msitest\\maximus", TRUE), "File not present\n");
    ok(delete_pf("msitest", FALSE), "Directory not present or not empty\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

error:
    RegCloseKey(hkey);

    delete_pf("msitest\\maximus", TRUE);
    delete_pf("msitest", FALSE);
    DeleteFileA("msitest\\maximus");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_file_in_use_cab(void)
{
    UINT r;
    HANDLE file;
    HKEY hkey;
    char path[MAX_PATH];

    if (on_win9x)
    {
        win_skip("Pending file renaming is implemented differently on Win9x and WinMe\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    RegOpenKeyExA(HKEY_LOCAL_MACHINE, session_manager, 0, KEY_ALL_ACCESS, &hkey);

    CreateDirectoryA("msitest", NULL);
    create_file("maximus", 500);
    create_cab_file("test1.cab", MEDIA_SIZE, "maximus\0");
    DeleteFile("maximus");

    create_database(msifile, fiuc_tables, sizeof(fiuc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    lstrcpy(path, PROG_FILES_DIR);
    lstrcat(path, "\\msitest");
    CreateDirectoryA(path, NULL);

    lstrcat(path, "\\maximus");
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    r = MsiInstallProductA(msifile, "REBOOT=ReallySuppress FULL=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS_REBOOT_REQUIRED, "Expected ERROR_SUCCESS_REBOOT_REQUIRED got %u\n", r);
    ok(!file_matches_data(path, "maximus"), "Expected file not to match\n");
    CloseHandle(file);
    ok(!file_matches_data(path, "maximus"), "Expected file not to match\n");

    process_pending_renames(hkey);
    RegCloseKey(hkey);

    ok(file_matches_data(path, "maximus"), "Expected file to match\n");
    ok(delete_pf("msitest\\maximus", TRUE), "File not present\n");
    ok(delete_pf("msitest", FALSE), "Directory not present or not empty\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

error:
    RegCloseKey(hkey);

    delete_pf("msitest\\maximus", TRUE);
    delete_pf("msitest", FALSE);
    DeleteFileA("msitest\\maximus");
    delete_cab_files();
    delete_test_files();
    DeleteFile(msifile);
}

static INT CALLBACK handler_a(LPVOID context, UINT type, LPCSTR msg)
{
    return IDOK;
}

static INT CALLBACK handler_w(LPVOID context, UINT type, LPCWSTR msg)
{
    return IDOK;
}

static INT CALLBACK handler_record(LPVOID context, UINT type, MSIHANDLE record)
{
    return IDOK;
}

static void test_MsiSetExternalUI(void)
{
    INSTALLUI_HANDLERA ret_a;
    INSTALLUI_HANDLERW ret_w;
    INSTALLUI_HANDLER_RECORD prev;
    UINT error;

    ret_a = MsiSetExternalUIA(handler_a, INSTALLLOGMODE_ERROR, NULL);
    ok(ret_a == NULL, "expected NULL, got %p\n", ret_a);

    ret_a = MsiSetExternalUIA(NULL, 0, NULL);
    ok(ret_a == handler_a, "expected %p, got %p\n", handler_a, ret_a);

    /* Not present before Installer 3.1 */
    if (!pMsiSetExternalUIRecord) {
        win_skip("MsiSetExternalUIRecord is not available\n");
        return;
    }

    error = pMsiSetExternalUIRecord(handler_record, INSTALLLOGMODE_ERROR, NULL, &prev);
    ok(!error, "MsiSetExternalUIRecord failed %u\n", error);
    ok(prev == NULL, "expected NULL, got %p\n", prev);

    prev = (INSTALLUI_HANDLER_RECORD)0xdeadbeef;
    error = pMsiSetExternalUIRecord(NULL, INSTALLLOGMODE_ERROR, NULL, &prev);
    ok(!error, "MsiSetExternalUIRecord failed %u\n", error);
    ok(prev == handler_record, "expected %p, got %p\n", handler_record, prev);

    ret_w = MsiSetExternalUIW(handler_w, INSTALLLOGMODE_ERROR, NULL);
    ok(ret_w == NULL, "expected NULL, got %p\n", ret_w);

    ret_w = MsiSetExternalUIW(NULL, 0, NULL);
    ok(ret_w == handler_w, "expected %p, got %p\n", handler_w, ret_w);

    ret_a = MsiSetExternalUIA(handler_a, INSTALLLOGMODE_ERROR, NULL);
    ok(ret_a == NULL, "expected NULL, got %p\n", ret_a);

    ret_w = MsiSetExternalUIW(handler_w, INSTALLLOGMODE_ERROR, NULL);
    ok(ret_w == NULL, "expected NULL, got %p\n", ret_w);

    prev = (INSTALLUI_HANDLER_RECORD)0xdeadbeef;
    error = pMsiSetExternalUIRecord(handler_record, INSTALLLOGMODE_ERROR, NULL, &prev);
    ok(!error, "MsiSetExternalUIRecord failed %u\n", error);
    ok(prev == NULL, "expected NULL, got %p\n", prev);

    ret_a = MsiSetExternalUIA(NULL, 0, NULL);
    ok(ret_a == NULL, "expected NULL, got %p\n", ret_a);

    ret_w = MsiSetExternalUIW(NULL, 0, NULL);
    ok(ret_w == NULL, "expected NULL, got %p\n", ret_w);

    prev = (INSTALLUI_HANDLER_RECORD)0xdeadbeef;
    error = pMsiSetExternalUIRecord(NULL, 0, NULL, &prev);
    ok(!error, "MsiSetExternalUIRecord failed %u\n", error);
    ok(prev == handler_record, "expected %p, got %p\n", handler_record, prev);

    error = pMsiSetExternalUIRecord(handler_record, INSTALLLOGMODE_ERROR, NULL, NULL);
    ok(!error, "MsiSetExternalUIRecord failed %u\n", error);

    error = pMsiSetExternalUIRecord(NULL, 0, NULL, NULL);
    ok(!error, "MsiSetExternalUIRecord failed %u\n", error);
}

static void test_feature_override(void)
{
    UINT r;
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\override.txt", 1000);
    create_file("msitest\\preselected.txt", 1000);
    create_file("msitest\\notpreselected.txt", 1000);
    create_database(msifile, fo_tables, sizeof(fo_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    r = MsiInstallProductA(msifile, "ADDLOCAL=override");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(pf_exists("msitest\\override.txt"), "file not installed\n");
    ok(!pf_exists("msitest\\preselected.txt"), "file installed\n");
    ok(!pf_exists("msitest\\notpreselected.txt"), "file installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\override.txt", TRUE), "file not removed\n");

    r = MsiInstallProductA(msifile, "preselect=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(pf_exists("msitest\\override.txt"), "file not installed\n");
    ok(pf_exists("msitest\\preselected.txt"), "file not installed\n");
    ok(!pf_exists("msitest\\notpreselected.txt"), "file installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\override.txt", TRUE), "file not removed\n");
    todo_wine {
    ok(delete_pf("msitest\\preselected.txt", TRUE), "file removed\n");
    ok(delete_pf("msitest", FALSE), "directory removed\n");
    }

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(pf_exists("msitest\\override.txt"), "file not installed\n");
    ok(pf_exists("msitest\\preselected.txt"), "file not installed\n");
    ok(!pf_exists("msitest\\notpreselected.txt"), "file installed\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\override.txt", TRUE), "file not removed\n");
    todo_wine {
    ok(delete_pf("msitest\\preselected.txt", TRUE), "file removed\n");
    ok(delete_pf("msitest", FALSE), "directory removed\n");
    }

    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\msitest", access);

error:
    DeleteFileA("msitest\\override.txt");
    DeleteFileA("msitest\\preselected.txt");
    DeleteFileA("msitest\\notpreselected.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_create_folder(void)
{
    UINT r;

    create_test_files();
    create_database(msifile, cf_tables, sizeof(cf_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    ok(!delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout\\new", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\cabout\\four.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\changed\\three.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\changed", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\first\\two.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\first", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\filename", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\service.exe", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "Directory created\n");

    r = MsiInstallProductA(msifile, "LOCAL=Two");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    ok(!delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout\\new", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\cabout\\four.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\changed\\three.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\changed", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\first\\two.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\first", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\filename", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\service.exe", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "Directory created\n");

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_remove_folder(void)
{
    UINT r;

    create_test_files();
    create_database(msifile, rf_tables, sizeof(rf_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    ok(!delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout\\new", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\cabout\\four.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\changed\\three.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\changed", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\first\\two.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\first", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\filename", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\service.exe", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "Directory created\n");

    r = MsiInstallProductA(msifile, "LOCAL=Two");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    ok(!delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout\\new", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\cabout\\four.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\cabout", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\changed\\three.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\changed", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\first\\two.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\first", FALSE), "Directory created\n");
    ok(!delete_pf("msitest\\filename", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\one.txt", TRUE), "File installed\n");
    ok(!delete_pf("msitest\\service.exe", TRUE), "File installed\n");
    ok(!delete_pf("msitest", FALSE), "Directory created\n");

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_start_services(void)
{
    UINT r;
    SC_HANDLE scm, service;
    BOOL ret;
    DWORD error = ERROR_SUCCESS;

    if (on_win9x)
    {
        win_skip("Services are not implemented on Win9x and WinMe\n");
        return;
    }
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm && GetLastError() == ERROR_ACCESS_DENIED)
    {
        skip("Not enough rights to perform tests\n");
        return;
    }
    ok(scm != NULL, "Failed to open the SC Manager\n");
    if (!scm) return;

    service = OpenService(scm, "Spooler", SC_MANAGER_ALL_ACCESS);
    if (!service && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
    {
        win_skip("The 'Spooler' service does not exist\n");
        CloseServiceHandle(scm);
        return;
    }
    ok(service != NULL, "Failed to open Spooler, error %d\n", GetLastError());
    if (!service) {
        CloseServiceHandle(scm);
        return;
    }

    ret = StartService(service, 0, NULL);
    if (!ret && (error = GetLastError()) != ERROR_SERVICE_ALREADY_RUNNING)
    {
        skip("Terminal service not available, skipping test\n");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    create_test_files();
    create_database(msifile, sss_tables, sizeof(sss_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "Directory not created\n");

    delete_test_files();
    DeleteFile(msifile);

    if (error == ERROR_SUCCESS)
    {
        SERVICE_STATUS status;

        scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        service = OpenService(scm, "Spooler", SC_MANAGER_ALL_ACCESS);

        ret = ControlService(service, SERVICE_CONTROL_STOP, &status);
        ok(ret, "ControlService failed %u\n", GetLastError());

        CloseServiceHandle(service);
        CloseServiceHandle(scm);
    }
}

static void test_delete_services(void)
{
    UINT r;

    if (on_win9x)
    {
        win_skip("Services are not implemented on Win9x and WinMe\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, sds_tables, sizeof(sds_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "Directory not created\n");

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_self_registration(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, sr_tables, sizeof(sr_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "Directory not created\n");

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_register_font(void)
{
    static const char regfont1[] = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    static const char regfont2[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Fonts";
    LONG ret;
    HKEY key;
    UINT r;
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\font.ttf", 1000);
    create_database(msifile, font_tables, sizeof(font_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, regfont1, 0, access, &key);
    if (ret)
        RegOpenKeyExA(HKEY_LOCAL_MACHINE, regfont2, 0, access, &key);

    ret = RegQueryValueExA(key, "msi test font", NULL, NULL, NULL, NULL);
    ok(ret != ERROR_FILE_NOT_FOUND, "unexpected result %d\n", ret);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

    ret = RegQueryValueExA(key, "msi test font", NULL, NULL, NULL, NULL);
    ok(ret == ERROR_FILE_NOT_FOUND, "unexpected result %d\n", ret);

    RegDeleteValueA(key, "msi test font");
    RegCloseKey(key);

error:
    DeleteFileA("msitest\\font.ttf");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_validate_product_id(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_database(msifile, vp_tables, sizeof(vp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "SET_PRODUCT_ID=1");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    r = MsiInstallProductA(msifile, "SET_PRODUCT_ID=2");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "PIDKEY=123-1234567");
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    ok(delete_pf("msitest\\cabout\\new\\five.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout\\new", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\cabout\\four.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\cabout", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\changed\\three.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\changed", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\first\\two.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\first", FALSE), "Directory not created\n");
    ok(delete_pf("msitest\\filename", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\one.txt", TRUE), "File not installed\n");
    ok(delete_pf("msitest\\service.exe", TRUE), "File not installed\n");
    ok(delete_pf("msitest", FALSE), "Directory not created\n");

error:
    delete_test_files();
    DeleteFile(msifile);
}

static void test_install_remove_odbc(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\ODBCdriver.dll", 1000);
    create_file("msitest\\ODBCdriver2.dll", 1000);
    create_file("msitest\\ODBCtranslator.dll", 1000);
    create_file("msitest\\ODBCtranslator2.dll", 1000);
    create_file("msitest\\ODBCsetup.dll", 1000);
    create_database(msifile, odbc_tables, sizeof(odbc_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(pf_exists("msitest\\ODBCdriver.dll"), "file not created\n");
    ok(pf_exists("msitest\\ODBCdriver2.dll"), "file not created\n");
    ok(pf_exists("msitest\\ODBCtranslator.dll"), "file not created\n");
    ok(pf_exists("msitest\\ODBCtranslator2.dll"), "file not created\n");
    ok(pf_exists("msitest\\ODBCsetup.dll"), "file not created\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\ODBCdriver.dll", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\ODBCdriver2.dll", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\ODBCtranslator.dll", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\ODBCtranslator2.dll", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\ODBCsetup.dll", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\ODBCdriver.dll");
    DeleteFileA("msitest\\ODBCdriver2.dll");
    DeleteFileA("msitest\\ODBCtranslator.dll");
    DeleteFileA("msitest\\ODBCtranslator2.dll");
    DeleteFileA("msitest\\ODBCsetup.dll");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_register_typelib(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\typelib.dll", 1000);
    create_database(msifile, tl_tables, sizeof(tl_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, "REGISTER_TYPELIB=1");
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_INSTALL_FAILURE, "Expected ERROR_INSTALL_FAILURE, got %u\n", r);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\typelib.dll", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\typelib.dll");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_create_remove_shortcut(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\target.txt", 1000);
    create_database(msifile, crs_tables, sizeof(crs_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(pf_exists("msitest\\target.txt"), "file not created\n");
    ok(pf_exists("msitest\\shortcut.lnk"), "file not created\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\shortcut.lnk", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\target.txt", TRUE), "file not removed\n");
    todo_wine ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\target.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_publish_components(void)
{
    static char keypath[] =
        "Software\\Microsoft\\Installer\\Components\\0CBCFA296AC907244845745CEEB2F8AA";

    UINT r;
    LONG res;
    HKEY key;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\english.txt", 1000);
    create_database(msifile, pub_tables, sizeof(pub_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CURRENT_USER, keypath, &key);
    ok(res == ERROR_SUCCESS, "components key not created %d\n", res);

    res = RegQueryValueExA(key, "english.txt", NULL, NULL, NULL, NULL);
    ok(res == ERROR_SUCCESS, "value not found %d\n", res);
    RegCloseKey(key);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CURRENT_USER, keypath, &key);
    ok(res == ERROR_FILE_NOT_FOUND, "unexpected result %d\n", res);

    ok(!delete_pf("msitest\\english.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\english.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_remove_duplicate_files(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\original.txt", 1000);
    create_file("msitest\\original2.txt", 1000);
    create_file("msitest\\original3.txt", 1000);
    create_database(msifile, rd_tables, sizeof(rd_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(pf_exists("msitest\\original.txt"), "file not created\n");
    ok(pf_exists("msitest\\original2.txt"), "file not created\n");
    ok(!pf_exists("msitest\\original3.txt"), "file created\n");
    ok(pf_exists("msitest\\duplicate.txt"), "file not created\n");
    ok(!pf_exists("msitest\\duplicate2.txt"), "file created\n");

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(delete_pf("msitest\\original.txt", TRUE), "file removed\n");
    ok(!delete_pf("msitest\\original2.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\original3.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\duplicate.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest\\duplicate2.txt", TRUE), "file not removed\n");
    ok(delete_pf("msitest", FALSE), "directory removed\n");

error:
    DeleteFileA("msitest\\original.txt");
    DeleteFileA("msitest\\original2.txt");
    DeleteFileA("msitest\\original3.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_remove_registry_values(void)
{
    UINT r;
    LONG res;
    HKEY key;
    REGSAM access = KEY_ALL_ACCESS;
    BOOL wow64;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\registry.txt", 1000);
    create_database(msifile, rrv_tables, sizeof(rrv_tables) / sizeof(msi_table));

    if (pIsWow64Process && pIsWow64Process(GetCurrentProcess(), &wow64) && wow64)
        access |= KEY_WOW64_64KEY;

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key1", 0, NULL, 0, access, NULL, &key, NULL);
    RegSetValueExA(key, "value1", 0, REG_SZ, (const BYTE *)"1", 2);
    RegCloseKey(key);

    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key2", 0, NULL, 0, access, NULL, &key, NULL);
    RegSetValueExA(key, "value2", 0, REG_SZ, (const BYTE *)"2", 2);
    RegCloseKey(key);

    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyA", 0, NULL, 0, access, NULL, &key, NULL);
    RegSetValueExA(key, "", 0, REG_SZ, (const BYTE *)"default", 8);
    RegSetValueExA(key, "valueA", 0, REG_SZ, (const BYTE *)"A", 2);
    RegSetValueExA(key, "valueB", 0, REG_SZ, (const BYTE *)"B", 2);
    RegCloseKey(key);

    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyB", 0, NULL, 0, access, NULL, &key, NULL);
    RegSetValueExA(key, "", 0, REG_SZ, (const BYTE *)"default", 8);
    RegSetValueExA(key, "valueB", 0, REG_SZ, (const BYTE *)"B", 2);
    RegCloseKey(key);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key1", 0, access, &key);
    ok(res == ERROR_SUCCESS, "key removed\n");
    RegCloseKey(key);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key2", 0, access, &key);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    res = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key2", 0, NULL, 0, access, NULL, &key, NULL);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    RegCloseKey(key);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key1", 0, access, &key);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\key2", 0, access, &key);
    ok(res == ERROR_SUCCESS, "key removed\n");
    RegCloseKey(key);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyA", 0, access, &key);
    ok(res == ERROR_SUCCESS, "key removed\n");
    RegCloseKey(key);

    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyB", 0, access, &key);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyA", access);
    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\key2", access);
    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine", access);

    ok(!delete_pf("msitest\\registry.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\key1", access);
    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\key2", access);
    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyA", access);
    delete_key(HKEY_LOCAL_MACHINE, "Software\\Wine\\keyB", access);

    DeleteFileA("msitest\\registry.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_find_related_products(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\product.txt", 1000);
    create_database(msifile, frp_tables, sizeof(frp_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    /* install again, so it finds the upgrade code */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\product.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\product.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_remove_ini_values(void)
{
    UINT r;
    DWORD len;
    char inifile[MAX_PATH], buf[0x10];
    HANDLE file;
    BOOL ret;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\inifile.txt", 1000);
    create_database(msifile, riv_tables, sizeof(riv_tables) / sizeof(msi_table));

    lstrcpyA(inifile, PROG_FILES_DIR);
    lstrcatA(inifile, "\\msitest");
    ret = CreateDirectoryA(inifile, NULL);
    if (!ret && GetLastError() == ERROR_ACCESS_DENIED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    lstrcatA(inifile, "\\test.ini");
    file = CreateFileA(inifile, GENERIC_WRITE|GENERIC_READ, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(file);

    ret = WritePrivateProfileStringA("section1", "key1", "value1", inifile);
    ok(ret, "failed to write profile string %u\n", GetLastError());

    ret = WritePrivateProfileStringA("sectionA", "keyA", "valueA", inifile);
    ok(ret, "failed to write profile string %u\n", GetLastError());

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    len = GetPrivateProfileStringA("section1", "key1", NULL, buf, sizeof(buf), inifile);
    ok(len == 6, "got %u expected 6\n", len);

    len = GetPrivateProfileStringA("sectionA", "keyA", NULL, buf, sizeof(buf), inifile);
    ok(!len, "got %u expected 0\n", len);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    len = GetPrivateProfileStringA("section1", "key1", NULL, buf, sizeof(buf), inifile);
    ok(!len, "got %u expected 0\n", len);

    len = GetPrivateProfileStringA("sectionA", "keyA", NULL, buf, sizeof(buf), inifile);
    ok(!len, "got %u expected 0\n", len);

    todo_wine ok(!delete_pf("msitest\\test.ini", TRUE), "file removed\n");
    ok(!delete_pf("msitest\\inifile.txt", TRUE), "file not removed\n");
    ok(delete_pf("msitest", FALSE), "directory removed\n");

error:
    DeleteFileA("msitest\\inifile.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_remove_env_strings(void)
{
    UINT r;
    LONG res;
    HKEY key;
    DWORD type, size;
    char buffer[0x10];

    if (on_win9x)
    {
        win_skip("Environment variables are handled differently on win9x and winme\n");
        return;
    }
    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\envvar.txt", 1000);
    create_database(msifile, res_tables, sizeof(res_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    res = RegOpenKeyA(HKEY_CURRENT_USER, "Environment", &key);
    ok(!res, "failed to open environment key %d\n", res);

    RegSetValueExA(key, "MSITESTVAR1", 0, REG_SZ, (const BYTE *)"1", 2);
    RegSetValueExA(key, "MSITESTVAR2", 0, REG_SZ, (const BYTE *)"1", 2);
    RegSetValueExA(key, "MSITESTVAR3", 0, REG_SZ, (const BYTE *)"1", 2);
    RegSetValueExA(key, "MSITESTVAR4", 0, REG_SZ, (const BYTE *)"1", 2);
    RegSetValueExA(key, "MSITESTVAR5", 0, REG_SZ, (const BYTE *)"1", 2);

    RegCloseKey(key);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CURRENT_USER, "Environment", &key);
    ok(!res, "failed to open environment key %d\n", res);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR1", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR2", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR3", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR4", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR5", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);

    RegCloseKey(key);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CURRENT_USER, "Environment", &key);
    ok(!res, "failed to open environment key %d\n", res);

    res = RegQueryValueExA(key, "MSITESTVAR1", NULL, NULL, NULL, NULL);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    res = RegQueryValueExA(key, "MSITESTVAR2", NULL, NULL, NULL, NULL);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR3", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);
    RegDeleteValueA(key, "MSITESTVAR3");

    res = RegQueryValueExA(key, "MSITESTVAR4", NULL, NULL, NULL, NULL);
    ok(res == ERROR_FILE_NOT_FOUND, "Expected ERROR_FILE_NOT_FOUND, got %d\n", res);

    type = REG_NONE;
    buffer[0] = 0;
    size = sizeof(buffer);
    res = RegQueryValueExA(key, "MSITESTVAR5", NULL, &type, (LPBYTE)buffer, &size);
    ok(res == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %d\n", res);
    ok(type == REG_SZ, "expected REG_SZ, got %u\n", type);
    ok(!lstrcmp(buffer, "1"), "expected \"1\", got \"%s\"\n", buffer);
    RegDeleteValueA(key, "MSITESTVAR5");

    ok(!delete_pf("msitest\\envvar.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    RegDeleteValueA(key, "MSITESTVAR1");
    RegDeleteValueA(key, "MSITESTVAR2");
    RegDeleteValueA(key, "MSITESTVAR3");
    RegDeleteValueA(key, "MSITESTVAR4");
    RegDeleteValueA(key, "MSITESTVAR5");
    RegCloseKey(key);

    DeleteFileA("msitest\\envvar.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_register_class_info(void)
{
    UINT r;
    LONG res;
    HKEY hkey;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\class.txt", 1000);
    create_database(msifile, rci_tables, sizeof(rci_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "CLSID\\{110913E7-86D1-4BF3-9922-BA103FCDDDFA}", &hkey);
    ok(res == ERROR_SUCCESS, "key not created\n");
    RegCloseKey(hkey);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "FileType\\{110913E7-86D1-4BF3-9922-BA103FCDDDFA}", &hkey);
    ok(res == ERROR_SUCCESS, "key not created\n");
    RegCloseKey(hkey);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "AppID\\{CFCC3B38-E683-497D-9AB4-CB40AAFE307F}", &hkey);
    ok(res == ERROR_SUCCESS, "key not created\n");
    RegCloseKey(hkey);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "CLSID\\{110913E7-86D1-4BF3-9922-BA103FCDDDFA}", &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "FileType\\{110913E7-86D1-4BF3-9922-BA103FCDDDFA}", &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "AppID\\{CFCC3B38-E683-497D-9AB4-CB40AAFE307F}", &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    ok(!delete_pf("msitest\\class.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\class.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_register_extension_info(void)
{
    UINT r;
    LONG res;
    HKEY hkey;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\extension.txt", 1000);
    create_database(msifile, rei_tables, sizeof(rei_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, ".extension", &hkey);
    ok(res == ERROR_SUCCESS, "key not created\n");
    RegCloseKey(hkey);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "Prog.Id.1\\shell\\Open\\command", &hkey);
    ok(res == ERROR_SUCCESS, "key not created\n");
    RegCloseKey(hkey);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, ".extension", &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "Prog.Id.1", &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    ok(!delete_pf("msitest\\extension.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\extension.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_register_mime_info(void)
{
    UINT r;
    LONG res;
    HKEY hkey;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\mime.txt", 1000);
    create_database(msifile, rmi_tables, sizeof(rmi_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(msifile, NULL);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "MIME\\Database\\Content Type\\mime/type", &hkey);
    ok(res == ERROR_SUCCESS, "key not created\n");
    RegCloseKey(hkey);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    res = RegOpenKeyA(HKEY_CLASSES_ROOT, "MIME\\Database\\Content Type\\mime/type", &hkey);
    ok(res == ERROR_FILE_NOT_FOUND, "key not removed\n");

    ok(!delete_pf("msitest\\mime.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

error:
    DeleteFileA("msitest\\mime.txt");
    delete_test_files();
    DeleteFile(msifile);
}

static void test_icon_table(void)
{
    MSIHANDLE hdb = 0, record;
    LPCSTR query;
    UINT res;
    CHAR path[MAX_PATH], win9xpath[MAX_PATH];
    static const char prodcode[] = "{7DF88A49-996F-4EC8-A022-BF956F9B2CBB}";

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_database(msifile, icon_base_tables, sizeof(icon_base_tables) / sizeof(msi_table));

    res = MsiOpenDatabase(msifile, MSIDBOPEN_TRANSACT, &hdb);
    ok(res == ERROR_SUCCESS, "failed to open db: %d\n", res);

    query = "CREATE TABLE `Icon` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL  PRIMARY KEY `Name`)";
    res = run_query( hdb, 0, query );
    ok(res == ERROR_SUCCESS, "Can't create Icon table: %d\n", res);

    create_file("icon.ico", 100);
    record = MsiCreateRecord(1);
    res = MsiRecordSetStream(record, 1, "icon.ico");
    ok(res == ERROR_SUCCESS, "Failed to add stream data to record: %d\n", res);
    DeleteFile("icon.ico");

    query = "INSERT INTO `Icon` (`Name`, `Data`) VALUES ('testicon', ?)";
    res = run_query(hdb, record, query);
    ok(res == ERROR_SUCCESS, "Insert into Icon table failed: %d\n", res);

    res = MsiCloseHandle(record);
    ok(res == ERROR_SUCCESS, "Failed to close record handle: %d\n", res);
    res = MsiDatabaseCommit(hdb);
    ok(res == ERROR_SUCCESS, "Failed to commit database: %d\n", res);
    res = MsiCloseHandle(hdb);
    ok(res == ERROR_SUCCESS, "Failed to close database: %d\n", res);

    /* per-user */
    res = MsiInstallProductA(msifile, "PUBLISH_PRODUCT=1");
    if (res == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        DeleteFile(msifile);
        return;
    }
    ok(res == ERROR_SUCCESS, "Failed to do per-user install: %d\n", res);

    lstrcpyA(path, APP_DATA_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, "Microsoft\\Installer\\");
    lstrcatA(path, prodcode);
    lstrcatA(path, "\\testicon");
    ok(file_exists(path), "Per-user icon file isn't where it's expected (%s)\n", path);

    res = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(res == ERROR_SUCCESS, "Failed to uninstall per-user\n");

    /* system-wide */
    res = MsiInstallProductA(msifile, "PUBLISH_PRODUCT=1 ALLUSERS=1");
    ok(res == ERROR_SUCCESS, "Failed to system-wide install: %d\n", res);

    /* win9x with MSI 2.0 installs the icon to a different folder, same as above */
    lstrcpyA(win9xpath, APP_DATA_DIR);
    lstrcatA(win9xpath, "\\");
    lstrcatA(win9xpath, "Microsoft\\Installer\\");
    lstrcatA(win9xpath, prodcode);
    lstrcatA(win9xpath, "\\testicon");

    lstrcpyA(path, WINDOWS_DIR);
    lstrcatA(path, "\\");
    lstrcatA(path, "Installer\\");
    lstrcatA(path, prodcode);
    lstrcatA(path, "\\testicon");
    ok(file_exists(path) || file_exists(win9xpath),
            "System-wide icon file isn't where it's expected (%s)\n", path);

    res = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(res == ERROR_SUCCESS, "Failed to uninstall system-wide\n");

    delete_pfmsitest_files();
    DeleteFile(msifile);
}

static void test_sourcedir_props(void)
{
    UINT r;

    if (is_process_limited())
    {
        skip("process is limited\n");
        return;
    }

    create_test_files();
    create_file("msitest\\sourcedir.txt", 1000);
    create_database(msifile, sd_tables, sizeof(sd_tables) / sizeof(msi_table));

    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);

    /* full UI, no ResolveSource action */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\sourcedir.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

    /* full UI, ResolveSource action */
    r = MsiInstallProductA(msifile, "ResolveSource=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\sourcedir.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* no UI, no ResolveSource action */
    r = MsiInstallProductA(msifile, NULL);
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\sourcedir.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

    /* no UI, ResolveSource action */
    r = MsiInstallProductA(msifile, "ResolveSource=1");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    r = MsiInstallProductA(msifile, "REMOVE=ALL");
    ok(r == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %u\n", r);

    ok(!delete_pf("msitest\\sourcedir.txt", TRUE), "file not removed\n");
    ok(!delete_pf("msitest", FALSE), "directory not removed\n");

    DeleteFileA("msitest\\sourcedir.txt");
    DeleteFile(msifile);
}

START_TEST(install)
{
    DWORD len;
    char temp_path[MAX_PATH], prev_path[MAX_PATH], log_file[MAX_PATH];
    STATEMGRSTATUS status;
    BOOL ret = FALSE;

    init_functionpointers();

    on_win9x = check_win9x();

    GetCurrentDirectoryA(MAX_PATH, prev_path);
    GetTempPath(MAX_PATH, temp_path);
    SetCurrentDirectoryA(temp_path);

    lstrcpyA(CURR_DIR, temp_path);
    len = lstrlenA(CURR_DIR);

    if(len && (CURR_DIR[len - 1] == '\\'))
        CURR_DIR[len - 1] = 0;

    get_system_dirs();
    get_user_dirs();

    /* Create a restore point ourselves so we circumvent the multitude of restore points
     * that would have been created by all the installation and removal tests.
     */
    if (pSRSetRestorePointA)
    {
        memset(&status, 0, sizeof(status));
        ret = notify_system_change(BEGIN_NESTED_SYSTEM_CHANGE, &status);
    }

    /* Create only one log file and don't append. We have to pass something
     * for the log mode for this to work. The logfile needs to have an absolute
     * path otherwise we still end up with some extra logfiles as some tests
     * change the current directory.
     */
    lstrcpyA(log_file, temp_path);
    lstrcatA(log_file, "\\msitest.log");
    MsiEnableLogA(INSTALLLOGMODE_FATALEXIT, log_file, 0);

    test_MsiInstallProduct();
    test_MsiSetComponentState();
    test_packagecoltypes();
    test_continuouscabs();
    test_caborder();
    test_mixedmedia();
    test_samesequence();
    test_uiLevelFlags();
    test_readonlyfile();
    test_readonlyfile_cab();
    test_setdirproperty();
    test_cabisextracted();
    test_concurrentinstall();
    test_setpropertyfolder();
    test_publish_registerproduct();
    test_publish_publishproduct();
    test_publish_publishfeatures();
    test_publish_registeruser();
    test_publish_processcomponents();
    test_publish();
    test_publishsourcelist();
    test_transformprop();
    test_currentworkingdir();
    test_admin();
    test_adminprops();
    test_removefiles();
    test_movefiles();
    test_missingcab();
    test_duplicatefiles();
    test_writeregistryvalues();
    test_sourcefolder();
    test_customaction51();
    test_installstate();
    test_sourcepath();
    test_MsiConfigureProductEx();
    test_missingcomponent();
    test_sourcedirprop();
    test_adminimage();
    test_propcase();
    test_int_widths();
    test_shortcut();
    test_envvar();
    test_lastusedsource();
    test_preselected();
    test_installed_prop();
    test_file_in_use();
    test_file_in_use_cab();
    test_MsiSetExternalUI();
    test_allusers_prop();
    test_feature_override();
    test_create_folder();
    test_remove_folder();
    test_start_services();
    test_delete_services();
    test_self_registration();
    test_register_font();
    test_validate_product_id();
    test_install_remove_odbc();
    test_register_typelib();
    test_create_remove_shortcut();
    test_publish_components();
    test_remove_duplicate_files();
    test_remove_registry_values();
    test_find_related_products();
    test_remove_ini_values();
    test_remove_env_strings();
    test_register_class_info();
    test_register_extension_info();
    test_register_mime_info();
    test_icon_table();
    test_sourcedir_props();

    DeleteFileA(log_file);

    if (pSRSetRestorePointA && ret)
    {
        ret = notify_system_change(END_NESTED_SYSTEM_CHANGE, &status);
        if (ret)
            remove_restore_point(status.llSequenceNumber);
    }
    FreeLibrary(hsrclient);

    SetCurrentDirectoryA(prev_path);
}
