// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.LanguageSplitInstaller;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Settings fragment that displays information about Chrome languages, which allow users to
 * seamlessly find and manage their languages preferences across platforms.
 */
public class LanguageSettings extends PreferenceFragmentCompat
        implements AddLanguageFragment.Launcher, FragmentSettingsLauncher {
    // Return codes from launching Intents on preferences.
    private static final int REQUEST_CODE_ADD_ACCEPT_LANGUAGE = 1;
    private static final int REQUEST_CODE_CHANGE_APP_LANGUAGE = 2;
    private static final int REQUEST_CODE_CHANGE_TARGET_LANGUAGE = 3;

    // The keys for each preference shown on the languages page.
    static final String APP_LANGUAGE_SECTION_KEY = "app_language_section";
    static final String APP_LANGUAGE_PREFERENCE_KEY = "app_language_preference";
    static final String PREFERRED_LANGUAGES_KEY = "preferred_languages";
    static final String CONTENT_LANGUAGES_KEY = "content_languages_preference";
    static final String TRANSLATE_SWITCH_KEY = "translate_switch";

    static final String TRANSLATION_ADVANCED_SECTION = "translation_advanced_settings_section";
    static final String TARGET_LANGUAGE_KEY = "translate_settings_target_language";
    static final String ALWAYS_LANGUAGES_KEY = "translate_settings_always_languages";
    static final String NEVER_LANGUAGES_KEY = "translate_settings_never_languages";

    private static final String TAG = "LanguageSettings";

    // SettingsLauncher injected from main Settings Activity.
    private SettingsLauncher mSettingsLauncher;
    private AppLanguagePreferenceDelegate mAppLanguageDelegate =
            new AppLanguagePreferenceDelegate();
    private PrefChangeRegistrar mPrefChangeRegistrar;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.language_settings);
        mPrefChangeRegistrar = new PrefChangeRegistrar();

        // Show the detailed language settings if DETAILED_LANGUAGE_SETTINGS feature is enabled.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS)) {
            createDetailedPreferences(savedInstanceState, rootKey);
        } else {
            createBasicPreferences(savedInstanceState, rootKey);
        }

        LanguagesManager.recordImpression(LanguagesManager.LanguageSettingsPageType.PAGE_MAIN);
    }

    /**
     * Create the old language and translate settings page.  Delete once no longer used.
     */
    private void createBasicPreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.languages_preferences);

        ContentLanguagesPreference mLanguageListPref =
                (ContentLanguagesPreference) findPreference(PREFERRED_LANGUAGES_KEY);
        mLanguageListPref.registerActivityLauncher(this);

        ChromeSwitchPreference translateSwitch =
                (ChromeSwitchPreference) findPreference(TRANSLATE_SWITCH_KEY);
        boolean isTranslateEnabled = getPrefService().getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
        translateSwitch.setChecked(isTranslateEnabled);

        translateSwitch.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                boolean enabled = (boolean) newValue;
                getPrefService().setBoolean(Pref.OFFER_TRANSLATE_ENABLED, enabled);
                mLanguageListPref.notifyPrefChanged();
                LanguagesManager.recordAction(enabled ? LanguagesManager.LanguageSettingsActionType
                                                                .ENABLE_TRANSLATE_GLOBALLY
                                                      : LanguagesManager.LanguageSettingsActionType
                                                                .DISABLE_TRANSLATE_GLOBALLY);
                return true;
            }
        });
        translateSwitch.setManagedPreferenceDelegate((ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED));
    }

    /**
     * Create the new language and translate settings page. With options to change the app language,
     * translate target language, and detailed translate preferences.
     */
    private void createDetailedPreferences(Bundle savedInstanceState, String rootKey) {
        // Log currently installed language splits.
        String installedLanguages =
                TextUtils.join(",", LanguageSplitInstaller.getInstance().getInstalledLanguages());
        Log.i(TAG, TextUtils.concat("Installed Languages: ", installedLanguages).toString());

        SettingsUtils.addPreferencesFromResource(this, R.xml.languages_detailed_preferences);

        setupAppLanguageSection();

        ContentLanguagesPreference mLanguageListPref =
                (ContentLanguagesPreference) findPreference(CONTENT_LANGUAGES_KEY);
        mLanguageListPref.registerActivityLauncher(this);

        setupTranslateSection(mLanguageListPref);
    }

    /**
     * Setup the App Language section with a title and preference to choose the app language.
     */
    private void setupAppLanguageSection() {
        // Set title to include current app name.
        PreferenceCategory mAppLanguageTitle =
                (PreferenceCategory) findPreference(APP_LANGUAGE_SECTION_KEY);
        String appName = BuildInfo.getInstance().hostPackageLabel;
        mAppLanguageTitle.setTitle(getResources().getString(R.string.app_language_title, appName));

        LanguageItemPickerPreference appLanguagePreference =
                (LanguageItemPickerPreference) findPreference(APP_LANGUAGE_PREFERENCE_KEY);
        appLanguagePreference.setLanguageItem(AppLocaleUtils.getAppLanguagePref());
        appLanguagePreference.useLanguageItemForTitle(true);
        setSelectLanguageLauncher(appLanguagePreference,
                AddLanguageFragment.LANGUAGE_OPTIONS_UI_LANGUAGES, REQUEST_CODE_CHANGE_APP_LANGUAGE,
                LanguagesManager.LanguageSettingsPageType.CHANGE_CHROME_LANGUAGE);

        mAppLanguageDelegate.setup(this, appLanguagePreference);
    }

    /**
     * Setup the translate preferences section.  A switch preferences controls if translate is
     * enabled/disabled and will hide all advanced settings when disabled.
     * @param contentLanguagesPreference ContentLanguagesPreference reference to update about state
     *         changes.
     */
    private void setupTranslateSection(ContentLanguagesPreference contentLanguagesPreference) {
        // Setup expandable advanced settings section.
        PreferenceCategory translationAdvancedSection =
                (PreferenceCategory) findPreference(TRANSLATION_ADVANCED_SECTION);
        translationAdvancedSection.setOnExpandButtonClickListener(() -> {
            // Lambda for PreferenceGroup.OnExpandButtonClickListener.
            LanguagesManager.recordImpression(
                    LanguagesManager.LanguageSettingsPageType.ADVANCED_LANGUAGE_SETTINGS);
        });
        translationAdvancedSection.setVisible(
                getPrefService().getBoolean(Pref.OFFER_TRANSLATE_ENABLED));

        // Setup target language preference.
        LanguageItemPickerPreference targetLanguagePreference =
                (LanguageItemPickerPreference) findPreference(TARGET_LANGUAGE_KEY);
        targetLanguagePreference.setLanguageItem(TranslateBridge.getTargetLanguageForChromium());
        setSelectLanguageLauncher(targetLanguagePreference,
                AddLanguageFragment.LANGUAGE_OPTIONS_TRANSLATE_LANGUAGES,
                REQUEST_CODE_CHANGE_TARGET_LANGUAGE,
                LanguagesManager.LanguageSettingsPageType.CHANGE_TARGET_LANGUAGE);

        // Setup always translate preference.
        LanguageItemListPreference alwaysTranslatePreference =
                (LanguageItemListPreference) findPreference(ALWAYS_LANGUAGES_KEY);
        alwaysTranslatePreference.setFragmentListDelegate(
                new AlwaysTranslateListFragment.ListDelegate());
        mPrefChangeRegistrar.addObserver(
                Pref.PREF_ALWAYS_TRANSLATE_LIST, alwaysTranslatePreference);
        setLanguageListPreferenceClickListener(alwaysTranslatePreference);

        // Setup never translate preference.
        LanguageItemListPreference neverTranslatePreference =
                (LanguageItemListPreference) findPreference(NEVER_LANGUAGES_KEY);
        neverTranslatePreference.setFragmentListDelegate(
                new NeverTranslateListFragment.ListDelegate());
        mPrefChangeRegistrar.addObserver(Pref.FLUENT_LANGUAGES, neverTranslatePreference);
        setLanguageListPreferenceClickListener(neverTranslatePreference);

        // Setup translate switch to toggle advanced section on and off.
        ChromeSwitchPreference translateSwitch =
                (ChromeSwitchPreference) findPreference(TRANSLATE_SWITCH_KEY);
        boolean isTranslateEnabled = getPrefService().getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
        translateSwitch.setChecked(isTranslateEnabled);

        translateSwitch.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                boolean enabled = (boolean) newValue;
                getPrefService().setBoolean(Pref.OFFER_TRANSLATE_ENABLED, enabled);
                contentLanguagesPreference.notifyPrefChanged();
                translationAdvancedSection.setVisible(enabled);
                LanguagesManager.recordAction(enabled ? LanguagesManager.LanguageSettingsActionType
                                                                .ENABLE_TRANSLATE_GLOBALLY
                                                      : LanguagesManager.LanguageSettingsActionType
                                                                .DISABLE_TRANSLATE_GLOBALLY);
                return true;
            }
        });
        translateSwitch.setManagedPreferenceDelegate((ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED));
    }

    @Override
    public void onStart() {
        super.onStart();
        mAppLanguageDelegate.maybeShowSnackbar();
    }

    @Override
    public void onDetach() {
        super.onDetach();
        LanguagesManager.recycle();
        mPrefChangeRegistrar.destroy();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != Activity.RESULT_OK) return;

        String code = data.getStringExtra(AddLanguageFragment.INTENT_SELECTED_LANGUAGE);
        if (requestCode == REQUEST_CODE_ADD_ACCEPT_LANGUAGE) {
            LanguagesManager.getInstance().addToAcceptLanguages(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.LANGUAGE_ADDED);
        } else if (requestCode == REQUEST_CODE_CHANGE_APP_LANGUAGE) {
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.CHANGE_CHROME_LANGUAGE);
            mAppLanguageDelegate.startLanguageSplitDownload(code);
        } else if (requestCode == REQUEST_CODE_CHANGE_TARGET_LANGUAGE) {
            LanguageItemPickerPreference targetLanguagePreference =
                    (LanguageItemPickerPreference) findPreference(TARGET_LANGUAGE_KEY);
            targetLanguagePreference.setLanguageItem(code);
            TranslateBridge.setDefaultTargetLanguage(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.CHANGE_TARGET_LANGUAGE);
        }
    }

    /**
     * Overrides AddLanguageFragment.Launcher.launchAddLanguage to handle click events on the
     * Add Language button inside the ContentLanguagesPreference.
     */
    @Override
    public void launchAddLanguage() {
        LanguagesManager.recordImpression(
                LanguagesManager.LanguageSettingsPageType.CONTENT_LANGUAGE_ADD_LANGUAGE);
        launchSelectLanguage(AddLanguageFragment.LANGUAGE_OPTIONS_ACCEPT_LANGUAGES,
                REQUEST_CODE_ADD_ACCEPT_LANGUAGE);
    }

    /**
     * Overrides FragmentSettingsLauncher.setSettingsLauncher to inject the App SettingsLauncher.
     * @param settingsLauncher App SettingsLauncher instance.
     */
    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    /**
     * Set the action to restart Chrome for the App Language Snackbar.
     */
    public void setRestartAction(AppLanguagePreferenceDelegate.RestartAction action) {
        mAppLanguageDelegate.setRestartAction(action);
    }

    /**
     * Set preference's OnPreferenceClickListener to launch the Select Language Fragment.
     * @param Preference preference The Preference to set listener on.
     * @param int launchCode The language options code to filter selectable languages.
     * @param int requestCode The code to return from the select language fragment with.
     * @param int pageType The LanguageSettingsPageType to record impression for.
     */
    private void setSelectLanguageLauncher(Preference preference, int launchCode, int requestCode,
            @LanguagesManager.LanguageSettingsPageType int pageType) {
        preference.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                LanguagesManager.recordImpression(pageType);
                launchSelectLanguage(launchCode, requestCode);
                return true;
            }
        });
    }

    /**
     * Launch the AddLanguageFragment with launch and request codes to select a single language.
     * @param int launchCode The language options code to filter selectable languages.
     * @param int requestCode The code to return from the select language fragment with.
     */
    private void launchSelectLanguage(int launchCode, int requestCode) {
        Intent intent = mSettingsLauncher.createSettingsActivityIntent(
                getActivity(), AddLanguageFragment.class.getName());
        intent.putExtra(AddLanguageFragment.INTENT_LANGUAGE_OPTIONS, launchCode);
        startActivityForResult(intent, requestCode);
    }

    /**
     * Sets the PreferenceClickListener on a {@link LanguageItemListPreference} to launch an intent
     * for {@link LanguageItemListFragment}.
     * @param listPreference LanguageItemListPreference to set preference click listener on.
     */
    private void setLanguageListPreferenceClickListener(LanguageItemListPreference listPreference) {
        listPreference.setOnPreferenceClickListener(preference -> {
            Intent intent = mSettingsLauncher.createSettingsActivityIntent(
                    getActivity(), listPreference.getFragmentClassName());
            startActivity(intent);
            return true;
        });
    }

    @VisibleForTesting
    static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}
