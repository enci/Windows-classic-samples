//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************


// DWriteCustomFontSets.cpp : Defines the entry point for the console application.

#include "stdafx.h"
#include "DWriteCustomFontSets.h"
#include "CommandLineArgs.h"


using namespace DWriteCustomFontSets;


int wmain(int argc, wchar_t* argv[])
{
    // Process the command line. If invalid arguments or help requested (returns false), display usage text and exit.
    CommandLineArgs args = CommandLineArgs();
    if (!args.ProcessArgs(argc, argv))
    {
        args.DisplayUsage();
        return -1;
    }

    Scenario scenario = args.GetScenario();


    // Got inputs. Now execute the various scenarios that create a custom font set.

    CustomFontSetManager fontSetManager = CustomFontSetManager();
    switch (scenario)
    {

    case DWriteCustomFontSets::Scenario::InMemoryFonts:
        // For this scenario, the method to create the font set will simulate a document containing
        // embedded font data that is extracted from the document into memory and then loaded into
        // a custom font set.
        //
        // Note: the implementation used requires Windows 10 Creators Update. Similar scenarios could
        // be supported in earlier Windows versions by implementing the IDWriteFontCollectionLoader
        // and other related interfaces. That implementation is not demonstrated in this sample.
        std::wcout << L"Scenario 4: custom font set using in-memory font data.\n";
        if (!fontSetManager.IDWriteFactory5_IsAvailable())
        {
            std::wcout << L"This scenario requires Windows 10 Creators Update (preview build 15021 or later).\n";
            return -1;
        }
        fontSetManager.CreateFontSetUsingInMemoryFontData();
        break;

    default:
        std::wcout << L"\nThe selected scenario is not implemented.\n";
        return -1;
    } // end switch(scenario)


    // Got a font set. Now report some basic font properties maintained in the font set. If the font 
    // set was created with custom properties defined for each font, this won't require actually
    // reading the font data.
    ReportFontProperties(fontSetManager);


    // The font properties reported by the previous line are contained in the font set object. If
    // custom properties were set, these may be different from actual values contained within the
    // actual font data. So, we'll report some additional details that come directly from the fonts.
    //
    // Also, if the font set contains references to remote fonts, it's important to understand how
    // to interact with DirectWrite APIs for handling actual download of the font data. Creating 
    // the font set alone doesn't require downloading. For the remote font scenario, the additional
    // font details being reported will require downloading of actual data.
    ReportFontDataDetails(fontSetManager);

    return 0;
} // end wmain




// ****************************************************
//
//    Methods for reporting information about each
//    font in the font set.
//
// ****************************************************

void DWriteCustomFontSets::ReportFontProperties(const CustomFontSetManager& fontSetManager)
{
    std::wcout << L"Number of fonts in the font set: " << fontSetManager.GetFontCount() << std::endl;

    // Check there are fonts present.
    if (fontSetManager.GetFontCount() > 0)
    {
        // For each font face reference, we'll report a full face name as a representative property.
        std::wcout << "\nFull face name property for fonts in the custom font set:\n";

        std::vector<std::wstring> fullNames = fontSetManager.GetFullFontNames();
        for (auto& fullName : fullNames)
        {
            std::wcout << fullName << std::endl;
        }
        std::wcout << std::endl;
    }

    return;
} // end ReportFontProperties()


void DWriteCustomFontSets::ReportFontDataDetails(const CustomFontSetManager& fontSetManager)
{
    // Check there are fonts present.
    if (fontSetManager.GetFontCount() > 0)
    {
        // For each font, report some details that will require actual font data.
        std::wcout << L"\nReporting some details requiring actual font data:\n";

        // For remote fonts, the additional details will require download of the font data, and
        // that will have unpredictable latency or success. In case of slow or no connection, 
        // the CustomFontSetManager::GetFontDataDetails implementation will timeout; but it 
        // also accepts an additional event handle we can use to allow the user to exit early.
        HANDLE inputHandle = nullptr;

        if (fontSetManager.CustomFontSetHasRemoteFonts())
        {
            std::wcout << L"The custom font set has remote fonts that will need to be downloaded.\n";
            // In a typical situation involving remote fonts, an app would proceed to display text
            // using local fonts as a fallback, and then refresh when the remote fonts are available.
            // (For an example of this, see the DWriteTextLayoutCloudFont sample at 
            // https://github.com/Microsoft/Windows-universal-samples/tree/master/Samples/DWriteTextLayoutCloudFont.)
            // In this case, we'll just wait, but will also give the user a chance to exit.
            inputHandle = GetStdHandle(STD_INPUT_HANDLE);
            if (inputHandle == INVALID_HANDLE_VALUE)
            {
                std::wcout << L"Unable to fetch the remote fonts without risk of blocking UI, so skipping.\n";
                return;
            }
            std::wcout << L"Fetching remote fonts, which may take some time. To quit, press any key...\n";
            FlushConsoleInputBuffer(inputHandle);
        }
        std::vector<std::wstring> fontDetails = fontSetManager.GetFontDataDetails(inputHandle);
        for (auto& fontDetailRow : fontDetails)
        {
            std::wcout << fontDetailRow << std::endl;
        }
    } // end if

    return;
} // end ReportFontDataDetails()

