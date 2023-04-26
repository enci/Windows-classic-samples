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


#include "stdafx.h"
#include "BinaryResources.h" // Used in CreateFontSetUsingInMemoryFontData() for loading font data embedded in the app binary.
#include "CustomFontSetManager.h"
#include "Document.h" // Used in CreateFontSetUsingInMemoryFontData() to simulate a document with embedded font data.


using Microsoft::WRL::ComPtr;


namespace DWriteCustomFontSets
{

    //**********************************************************************
    //
    //   Constructors, destructors
    //
    //**********************************************************************

    CustomFontSetManager::CustomFontSetManager()
    {
        HRESULT hr;

        // IDWriteFactory3 supports APIs available in any Windows 10 version (build 10240 or later).
        DX::ThrowIfFailed(
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory3), &m_dwriteFactory3)
        );

#ifndef FORCE_TH1_IMPLEMENTATION
        // IDWriteFactory5 supports APIs available in Windows 10 Creators Update (preview build 15021 or later).
        hr = m_dwriteFactory3.As(&m_dwriteFactory5);
        if (hr == E_NOINTERFACE)
        {
            // Let this go. Later, if we might use the interface, we'll branch gracefully.
        }
        else
        {
            DX::ThrowIfFailed(hr);
        }
#endif // !FORCE_TH1_IMPLEMENTATION

    } // end CustomFontSetManager::CustomFontSetManager()


    CustomFontSetManager::~CustomFontSetManager()
    {        
        // This will be relevant in scenario 4, when CreateFontSetUsingInMemoryFontData() is called.
        UnregisterFontFileLoader(m_inMemoryFontFileLoader.Get());        
    } // end CustomFontSetManager::~CustomFontSetManager()




      //**********************************************************************
      //
      //   Method for checking API availability.
      //
      //**********************************************************************

    bool CustomFontSetManager::IDWriteFactory5_IsAvailable()
    {
        return m_dwriteFactory5 != nullptr;
    }

    void CustomFontSetManager::CreateFontSetUsingInMemoryFontData()
    {
        // Requires Windows 10 Creators Update (preview build 15021 or later).

        // Creates a custom font set using in-memory font data.
        //
        // The implementation will use in-memory font data from two sources:
        //
        //   - a font embedded within the app binary as a resource; and
        //   - a document with embedded font data.
        //
        // These are two common app scenarios, but the implementation can be adapted
        // to other scenarios in which font data is loaded into memory.
        //
        // The BinaryResources class handles loading of the font embedded in the app
        // as a binary resource.
        //
        // The Document class simulates a document with embedded font data. In a real
        // scenario, the document would be read from a stream. As a simplification,
        // this simulation uses static data.
        //
        // The data in the memory buffer is expected to be raw, OpenType font data,
        // not data in a compressed, packed format such as WOFF2. For support of
        // packed-format font data, see scenario 5.

        // This will use a system implementation of IDWriteInMemoryFontFileLoader.
        // Before a font file loader can be used, it must be registered with a
        // DirectWrite factory object. The loader will be needed for as long as the
        // fonts may be used within the app, and so it will be stored as a
        // CustomFontSetManager member. It must be unregistered before it goes out of
        // scope; that will be done in the CustomFontSetManager destructor.


        // Get and register the system-implemented in-memory font file loader.
        DX::ThrowIfFailed(
            m_dwriteFactory5->CreateInMemoryFontFileLoader(&m_inMemoryFontFileLoader)
        );
        DX::ThrowIfFailed(
            m_dwriteFactory5->RegisterFontFileLoader(m_inMemoryFontFileLoader.Get())
        );

        // Get a font set builder. We're already dependent on Windows 10 Creators Update, 
        // so will use IDWriteFontSetBuilder1, which will save work later (won't need to 
        // check for an OpenType collection and loop over the individual fonts in the
        // collection).
        ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
        DX::ThrowIfFailed(
            m_dwriteFactory5->CreateFontSetBuilder(&fontSetBuilder)
        );


        // Load fonts embedded in the app binary as resources into memory.
        ComPtr<BinaryResources> binaryResources = new BinaryResources();
        std::vector<MemoryFontInfo> appFontResources;
        binaryResources->GetFonts(appFontResources);

        // Add the in-memory fonts to the font set.
        for (uint32_t fontIndex = 0; fontIndex < appFontResources.size(); fontIndex++)
        {
            MemoryFontInfo fontInfo = appFontResources[fontIndex];

            // For each in-memory font, get an IDWriteFontFile using the in-memory font
            // file loader. Then use that to get an IDWriteFontFaceReference, and add
            // the font face reference to the font set.

            ComPtr<IDWriteFontFile> fontFileReference;
            DX::ThrowIfFailed(
                m_inMemoryFontFileLoader->CreateInMemoryFontFileReference(
                    m_dwriteFactory5.Get(),
                    fontInfo.fontData,
                    fontInfo.fontDataSize,
                    binaryResources.Get(), // Passing the binaryResources object as the data owner -- data lifetime is managed by the owner, so DirectWrite won't make a copy.
                    &fontFileReference
                )
            );

            // The data may represent an OpenType collection file, which would include multiple
            // fonts. By using IDWriteFontSetBuilder1::AddFontFile, all of the fonts in a 
            // collection and all of the named instances in variable fonts are added in a single
            // call.

            // We're assuming here that the in-memory data is font data in a supported format.
            // Otherwise, we should check for the AddFontFile call failing with error 
            // DWRITE_E_FILEFORMAT.

            // Since the fonts are embedded in an app binary, they are known in advance, and so
            // custom font properties could be used. In that case, the custom properties would 
            // be specified here, and AddFontFaceReference would be used instead of AddFontFile.
            // See CreateFontSetUsingKnownAppFonts in this file for how that would be done.

            DX::ThrowIfFailed(
                fontSetBuilder->AddFontFile(fontFileReference.Get())
            );
        }

        // Get our simulated document that has embedded font data, and get the document
        // text and a vector of embedded font data.
        ComPtr<Documents::Document> document = new Documents::Document();
        std::wstring text = document->GetText();
        std::vector<MemoryFontInfo> documentFonts;
        document->GetFonts(documentFonts);

        // Add the in-memory fonts to the font set.
        for (uint32_t fontIndex = 0; fontIndex < documentFonts.size(); fontIndex++)
        {
            MemoryFontInfo fontInfo = documentFonts[fontIndex];

            ComPtr<IDWriteFontFile> fontFileReference;
            DX::ThrowIfFailed(
                m_inMemoryFontFileLoader->CreateInMemoryFontFileReference(
                    m_dwriteFactory5.Get(),
                    fontInfo.fontData,
                    fontInfo.fontDataSize,
                    document.Get(), // Passing the document object as the data owner -- data lifetime is managed by the owner, so DirectWrite won't make a copy.
                    &fontFileReference
                )
            );

            DX::ThrowIfFailed(
                fontSetBuilder->AddFontFile(fontFileReference.Get())
            );

        } // end for -- loop over fonts

          // Now create the custom font set.
        DX::ThrowIfFailed(
            fontSetBuilder->CreateFontSet(&m_customFontSet)
        );

        return;
    } // CustomFontSetManager::CreateFontSetUsingInMemoryFontData()


    //***********************************************************
    //
    //   Other public methods
    //
    //***********************************************************

    uint32_t CustomFontSetManager::GetFontCount() const
    {
        if (m_customFontSet == nullptr)
            return 0;

        return m_customFontSet->GetFontCount();
    }


    std::vector<std::wstring> CustomFontSetManager::GetFullFontNames() const
    {
        // Call GetPropertyValuesFromFontSet to get an IDWriteStringList with en-US (or default) full
        // names. IDWriteStringList is a dictionary with entries that include a language tag and the
        // property value. We only care about the latter.

        ComPtr<IDWriteStringList> fullNamePropertyValues = GetPropertyValuesFromFontSet(DWRITE_FONT_PROPERTY_ID_FULL_NAME);

        std::vector<std::wstring> fullNames;
        for (UINT32 i = 0; i < fullNamePropertyValues->GetCount(); i++)
        {
            std::wstring propertyValueString;
            UINT32 length;
            DX::ThrowIfFailed(
                fullNamePropertyValues->GetStringLength(i, &length)
            );
            propertyValueString.resize(length);
            DX::ThrowIfFailed(
                fullNamePropertyValues->GetString(i, &propertyValueString[0], length + 1)
            );
            fullNames.push_back(std::move(propertyValueString)); // Use move to avoid a copy.
        }

        return fullNames;
    } // end CustomFontSetManager::GetFullFontNames()


    std::vector<std::wstring> CustomFontSetManager::GetFontDataDetails(HANDLE cancellationHandle) const
    {
        // Report some representative details that require actual font data. If fonts are remote, a
        // download will be required. Since latency or success are uncertain, we'll give some ways to
        // interrupt the operation. The cancellationHandle parameter is for a caller-determined object
        // that we can wait on. We'll also set a 15-second timeout. In either case, we'll return an
        // empty vector.

        std::vector<std::wstring> resultVector;

        // We'll enqueue a download request for data from each font in the font set. If the font is
        // already local, this will be a no-op.
        //
        // Note that, depending on actual app scenarios, direct enqueueing may not be a typical usage
        // pattern. For instance, in apps that display text using IDWriteTextLayout, the layout will
        // automatically enqueue download requests as needed when measuring or drawing actions are
        // done, using fallback fonts in the meantime. After drawing or getting metrics, the app can
        // then check the font download queue to see if its non-empty, and initiate a download if
        // needed.

        for (uint32_t fontIndex = 0; fontIndex < m_customFontSet->GetFontCount(); fontIndex++)
        {
            ComPtr<IDWriteFontFaceReference> fontFaceReference;
            DX::ThrowIfFailed(
                m_customFontSet->GetFontFaceReference(fontIndex, &fontFaceReference)
            );
            DX::ThrowIfFailed(
                fontFaceReference->EnqueueFontDownloadRequest()
            );
        } // end for loop


        // Get representative data for each font: list the full name to identify the font (this will come directly 
        // from the font data, not from any custom font set properties), and give the font's x-height.
        for (uint32_t fontIndex = 0; fontIndex < m_customFontSet->GetFontCount(); fontIndex++)
        {
            std::wstring fontDetailReport;

            // Get IDWriteFontFace3
            ComPtr<IDWriteFontFaceReference> fontFaceReference;
            DX::ThrowIfFailed(
                m_customFontSet->GetFontFaceReference(fontIndex, &fontFaceReference)
            );
            ComPtr<IDWriteFontFace3> fontFace; // IDWriteFontFace3 or later is needed for the GetInformationalStrings() method.
            DX::ThrowIfFailed(
                fontFaceReference->CreateFontFace(&fontFace)
            );

            // Font detail report string begins with full name to identify the font.
            ComPtr<IDWriteLocalizedStrings> localizedStrings;
            BOOL exists;
            DX::ThrowIfFailed(
                fontFace->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_FULL_NAME, &localizedStrings, &exists)
            );
            if (exists) // should always be the case
            {
                uint32_t stringIndex;
                DX::ThrowIfFailed(
                    localizedStrings->FindLocaleName(L"en-US", &stringIndex, &exists)
                );
                if (!exists)
                {
                    stringIndex = 0;
                }
                uint32_t stringLength;
                DX::ThrowIfFailed(
                    localizedStrings->GetStringLength(stringIndex, &stringLength)
                );
                fontDetailReport.resize(stringLength);
                DX::ThrowIfFailed(
                    localizedStrings->GetString(stringIndex, &fontDetailReport[0], static_cast<UINT32>(fontDetailReport.size() + 1))
                );
                fontDetailReport.append(L": ");
            }
            else // In case we didn't get the full name, just give the font set index.
            {
                fontDetailReport.assign(L"Font ");
                fontDetailReport.append(std::to_wstring(fontIndex));
                fontDetailReport.append(L": ");
            }

            // Add to the font detail report the font's x-height.
            DWRITE_FONT_METRICS1 fontMetrics;
            fontFace->GetMetrics(&fontMetrics);
            fontDetailReport.append(L"x-height = ");
            fontDetailReport.append(std::to_wstring(fontMetrics.xHeight));

            // Add font detail report string to the result vector.
            resultVector.push_back(fontDetailReport);

        } // end for loop

        return resultVector;
    } // end CustomFontSetManager::GetFontDataDetails()


    bool CustomFontSetManager::CustomFontSetHasRemoteFonts() const
    {
        // Used to determine if there are any fonts in the font set for which data is currently
        // remote, thus requiring a download before it can be used. If all the data is already
        // local (was always local or has already been downloaded), then this will return FALSE.

        for (uint32_t fontIndex = 0; fontIndex < m_customFontSet->GetFontCount(); fontIndex++)
        {
            ComPtr<IDWriteFontFaceReference> fontFaceReference;
            DX::ThrowIfFailed(
                m_customFontSet->GetFontFaceReference(fontIndex, &fontFaceReference)
            );
            if (fontFaceReference->GetLocality() != DWRITE_LOCALITY_LOCAL)
                return true;
        }
        return false;
    } // end CustomFontSetHasRemoteFonts



    //***********************************************************
    //
    //   Private helper methods
    //
    //***********************************************************

    void CustomFontSetManager::UnregisterFontFileLoader(IDWriteFontFileLoader* fontFileLoader)
    {
        if (fontFileLoader != nullptr)
        {
            // Will be call from destructor. Ignore any errors.
            m_dwriteFactory3->UnregisterFontFileLoader(fontFileLoader);
        }
    }


    ComPtr<IDWriteStringList> CustomFontSetManager::GetPropertyValuesFromFontSet(DWRITE_FONT_PROPERTY_ID propertyId) const
    {
        // We can iterate over the font faces within the font set, but IDWriteFontSet has a convenient
        // GetPropertyValues method that allows us to get a list of information-string property values
        // from all of the fonts in the font set in one call. 
        //
        // A font can have multiple localized variants for a given informational string. If we call 
        // using the overload that doesn't include a parameter for language preference, the returned 
        // list will include all localized variants from all fonts. But if we indicate a language
        // preference, the list will include only the best language match from a font, with en-US as 
        // a fallback.
        //
        // The list includes unique values from across the set, so in general isn't guaranteed to have
        // as many values as there are fonts. Certain properties -- full name or Postscript name --
        // are likely to be unique to each font, however.

        ComPtr<IDWriteStringList> propertyValues;
        DX::ThrowIfFailed(
            m_customFontSet->GetPropertyValues(propertyId, L"en-US", &propertyValues)
        );

        return propertyValues;

    } // end CustomFontSetManager::GetPropertyValuesFromFontSet()


} // namespace DWriteCustomFontSets
