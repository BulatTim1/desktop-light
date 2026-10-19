// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const read=name=>fs.readFileSync(path.join(root,name),'utf8').replaceAll('\r\n','\n');
let state=read('src/native-popup-state.hpp').replace('#pragma once','').replace('#include "brightness-core.hpp"','');
const header=`// ==WindhawkMod==
// @id              cursor-brightness-native-popup
// @name            Cursor brightness in the native Windows popup
// @description     Synchronize the existing Windows brightness indicator with the cursor monitor
// @version         1.2.1
// @author          Cursor Brightness contributors
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lruntimeobject -luuid
// ==/WindhawkMod==
// SPDX-License-Identifier: GPL-3.0-only
`;
fs.writeFileSync(path.join(root,'mods/cursor-brightness-native-popup.wh.cpp'),header+read('src/brightness-core.hpp').replace('#pragma once','')+'\n'+state+'\n'+read('src/native-popup-mod.inc'));
console.log('Generated native Windows brightness popup extension');
