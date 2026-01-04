#include <unordered_set>

PlayerCharacter* player;
TESGlobal* diseaseCountGlobal;
TESGlobal* doneQuestsCountGlobal;
BGSListForm* diseasesList;
BGSListForm* doneQuests;
BGSKeyword* fortifyHealth;
bool bTaskQueued = false;

void UpdateDiseaseCount() {
    int count = 0;
    const auto& effects = player->AsMagicTarget()->GetActiveEffectList();
    std::unordered_set<const MagicItem*> seenSpells;
    for (auto it = effects->begin(); it != effects->end(); ++it) {
        auto& value = *it;
        if (value->spell && value->spell->GetSpellType() == MagicSystem::SpellType::kDisease) {
            if (seenSpells.insert(value->spell).second) {
                count++;
            }
        }
    }

    diseaseCountGlobal->value = count;
    bTaskQueued = false;
}

void MaybeUpdateDiseaseCount() {
    if (bTaskQueued) return;
    bTaskQueued = true;
    SKSE::GetTaskInterface()->AddTask(UpdateDiseaseCount);
}

class theSink : public BSTEventSink<TESMagicEffectApplyEvent>, public BSTEventSink<TESQuestStageEvent> {
    BSEventNotifyControl ProcessEvent(const TESMagicEffectApplyEvent* event,
                                      BSTEventSource<TESMagicEffectApplyEvent>*) {
        if (event->target && event->target->IsPlayerRef()) {
            MaybeUpdateDiseaseCount();
        }
        return BSEventNotifyControl::kContinue;
    }

    BSEventNotifyControl ProcessEvent(const TESQuestStageEvent* event, BSTEventSource<TESQuestStageEvent>*) {
        if (TESQuest* quest = TESForm::LookupByID<TESQuest>(event->formID)) {
            if (quest->IsCompleted() && quest->data.questType != QUEST_DATA::Type::kNone &&
                !quest->objectives.empty() && !doneQuests->HasForm(quest)) {
                doneQuestsCountGlobal->value += 1;
                doneQuests->AddForm(quest);
            }
        }
        return BSEventNotifyControl::kContinue;
    }
};

bool IsHealthModifierEffect(EffectSetting* a_effect) {
    auto type = a_effect->GetArchetype();
    if ((type == EffectArchetypes::ArchetypeID::kPeakValueModifier ||
            type == EffectArchetypes::ArchetypeID::kValueModifier ||
            type == EffectArchetypes::ArchetypeID::kDualValueModifier)
        && (a_effect->data.primaryAV == ActorValue::kHealth || a_effect->data.secondaryAV == ActorValue::kHealth)
    ) {
        return true;
    }
    return false;
}

void Setup() {
    auto& allSpells = TESDataHandler::GetSingleton()->GetFormArray<SpellItem>();
    for (auto* spell : allSpells) {
        if (spell && spell->GetSpellType() == MagicSystem::SpellType::kDisease) {
            diseasesList->AddForm(spell);
        }
    }
}

bool espLoaded = false;

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            diseasesList = TESForm::LookupByEditorID<BGSListForm>("LoreTraits_DiseaseSpellsList");
            if (diseasesList) {
                espLoaded = true;
            }
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            if (!espLoaded) return;
            Setup();
            static bool init = false;
            if (init) return;
            diseaseCountGlobal = TESForm::LookupByEditorID<TESGlobal>("LoreTraits_PlayerDiseaseCount");
            if (!diseaseCountGlobal) return;
            doneQuestsCountGlobal = TESForm::LookupByEditorID<TESGlobal>("LoreTraits_DoneQuestsCount");
            doneQuests = TESForm::LookupByEditorID<BGSListForm>("LoreTraits_DoneQuestsList");
            // fortifyHealth = TESForm::LookupByEditorID<BGSKeyword>("LoreTraits_FortifyHealthEffect");
            player = RE::PlayerCharacter::GetSingleton();
            static theSink g_sink;
            ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESMagicEffectApplyEvent>(&g_sink);
            ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESQuestStageEvent>(&g_sink);
            init = true;
        }
    });

    return true;
}