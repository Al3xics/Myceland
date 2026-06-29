// Myceland - creation des events SFX listes dans Game Content.xlsx.
// FMOD Studio 2.03 : Scripts > Reload, puis Myceland > Create SFX Events.
// Le script est idempotent : il conserve les events existants et ne cree que les manquants.

var mycelandGlobal = this;

(function (global) {
    "use strict";

    var MENU_NAME = "Myceland\\Create SFX Events";
    var BANK_NAME = "Gameplay";

    var EVENT_SPECS = [
        // Feedbacks - Avatar
        { folder: "Events/Gameplay", name: "Avatar_Footstep_Dirt", spatialized: true },
        { folder: "Events/Feedback", name: "Tile_Nature_Place_Success", spatialized: false },
        { folder: "Events/Feedback", name: "Tile_Placement_Invalid", spatialized: false },
        { folder: "Events/Feedback", name: "Energy_Collect", spatialized: false },
        { folder: "Events/Gameplay", name: "Avatar_Engulfed_Vocal", spatialized: true },
        { folder: "Events/Gameplay", name: "Avatar_Surprise_Low", spatialized: true },
        { folder: "Events/Gameplay", name: "Avatar_Surprise_Medium", spatialized: true },
        { folder: "Events/Gameplay", name: "Avatar_Surprise_High", spatialized: true },
        { folder: "Events/Gameplay", name: "Avatar_Victory_Vocal", spatialized: true },
        { folder: "Events/Feedback", name: "Timeline_Undo", spatialized: false },
        { folder: "Events/Feedback", name: "Timeline_Reset", spatialized: false },

        // Feedbacks - Tuiles
        { folder: "Events/Gameplay", name: "Tile_Plant", spatialized: true },
        { folder: "Events/Gameplay", name: "Tile_Parasite_Spread", spatialized: true },
        { folder: "Events/Gameplay", name: "Tile_Parasite_Engulf", spatialized: true },
        { folder: "Events/Gameplay", name: "Tile_Parasite_Die_Water", spatialized: true },
        { folder: "Events/Gameplay", name: "Tile_Earth_Dig", spatialized: true },
        { folder: "Events/Gameplay", name: "Tile_Water_Fill", spatialized: true },
        { folder: "Events/Gameplay", name: "Tile_Bridge_Grow", spatialized: true },
        { folder: "Events/Feedback", name: "Reaction_Chain_Nature", spatialized: false },
        { folder: "Events/Feedback", name: "Reaction_Chain_Parasite", spatialized: false },
        { folder: "Events/Feedback", name: "Reaction_Chain_Water", spatialized: false },
        { folder: "Events/Feedback", name: "Energy_Spawn", spatialized: false },
        { folder: "Events/Feedback", name: "Tree_Link_Motif", spatialized: false }
    ];

    function findBankByName(name) {
        var banks = studio.project.model.Bank.findInstances();
        for (var i = 0; i < banks.length; i++) {
            if (banks[i].name === name) {
                return banks[i];
            }
        }
        return null;
    }

    function getOrCreateEventFolder(path) {
        var root = studio.project.workspace.masterEventFolder;
        var parts = path.split("/");
        var current = root;
        var currentPath = "";

        for (var i = 0; i < parts.length; i++) {
            currentPath = currentPath ? currentPath + "/" + parts[i] : parts[i];
            var existing = root.getItem(currentPath);

            if (existing) {
                if (!existing.isOfExactType("EventFolder")) {
                    throw new Error("Le chemin '" + currentPath + "' existe mais n'est pas un dossier d'events.");
                }
                current = existing;
            } else {
                var folder = studio.project.create("EventFolder");
                folder.name = parts[i];
                folder.folder = current;
                current = folder;
            }
        }

        return current;
    }

    function hasBank(event, bank) {
        var destinations = event.relationships.banks.destinations;
        for (var i = 0; i < destinations.length; i++) {
            if (destinations[i].id === bank.id) {
                return true;
            }
        }
        return false;
    }

    function ensureAudioTrack(event) {
        var groupTracks = event.relationships.groupTracks.destinations;
        var masterModules = event.masterTrack.relationships.modules.destinations;

        if (groupTracks.length === 0 && masterModules.length === 0) {
            event.addGroupTrack("Audio");
            return true;
        }
        return false;
    }

    function ensureEvent(spec, bank, result) {
        var root = studio.project.workspace.masterEventFolder;
        var eventPath = spec.folder + "/" + spec.name;
        var event = root.getItem(eventPath);

        if (!event) {
            var folder = getOrCreateEventFolder(spec.folder);
            event = studio.project.workspace.addEvent(spec.name, spec.spatialized);
            event.folder = folder;
            event.addGroupTrack("Audio");
            result.created.push("event:/" + eventPath);
        } else {
            if (!event.isOfExactType("Event")) {
                throw new Error("Le chemin '" + eventPath + "' existe mais n'est pas un event.");
            }
            result.kept.push("event:/" + eventPath);

            if (ensureAudioTrack(event)) {
                result.updated.push("Ajout d'une piste Audio : event:/" + eventPath);
            }
        }

        if (!hasBank(event, bank)) {
            event.relationships.banks.add(bank);
            result.updated.push("Affecte a la bank " + BANK_NAME + " : event:/" + eventPath);
        }

        if (spec.spatialized && !event.is3D()) {
            event.masterTrack.mixerGroup.effectChain.addEffect("SpatialiserEffect");
            if (event.is3D()) {
                result.updated.push("Spatializer ajoute : event:/" + eventPath);
            } else {
                result.warnings.push("A verifier en 3D : event:/" + eventPath);
            }
        }
    }

    function createSfxEvents(silent) {
        var bank = findBankByName(BANK_NAME);
        if (!bank) {
            var bankError = "Bank '" + BANK_NAME + "' introuvable. Cree-la dans FMOD puis relance le script.";
            if (silent) {
                throw new Error(bankError);
            }
            alert(bankError);
            return bankError;
        }

        var result = { created: [], kept: [], updated: [], warnings: [] };

        try {
            for (var i = 0; i < EVENT_SPECS.length; i++) {
                ensureEvent(EVENT_SPECS[i], bank, result);
            }

            studio.project.save();

            var message =
                "Events SFX traites : " + EVENT_SPECS.length + "\n" +
                "Crees : " + result.created.length + "\n" +
                "Conserves : " + result.kept.length + "\n" +
                "Mis a jour : " + result.updated.length;

            if (result.warnings.length > 0) {
                message += "\n\nAttention :\n- " + result.warnings.join("\n- ");
            }

            console.log("[Myceland SFX] " + message);
            if (result.created.length > 0) {
                console.log("[Myceland SFX] Crees :\n" + result.created.join("\n"));
            }
            if (result.updated.length > 0) {
                console.log("[Myceland SFX] Mis a jour :\n" + result.updated.join("\n"));
            }
            if (!silent) {
                alert(message);
            }
            return message;
        } catch (error) {
            console.error("[Myceland SFX] " + error);
            if (silent) {
                throw error;
            }
            alert("Creation interrompue : " + error);
            return "Creation interrompue : " + error;
        }
    }

    global.createMycelandSfxEvents = function () {
        return createSfxEvents(true);
    };

    studio.menu.addMenuItem({
        name: MENU_NAME,
        execute: function () { createSfxEvents(false); }
    });
}(mycelandGlobal));
