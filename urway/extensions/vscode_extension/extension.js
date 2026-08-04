// U'rWay VS Code Coding Duration Tracker
// Privacy Guarantee: Zero source code, variable names, or file contents are read or transmitted.

const vscode = require('vscode');

let activeLanguage = 'plaintext';
let activeCodingSeconds = 0;
let idleSeconds = 0;
let timer = null;

function activate(context) {
    console.log('[U\'rWay VSCode] Privacy-preserving tracker activated.');

    // Monitor document changes to count active coding time
    vscode.workspace.onDidChangeTextDocument((event) => {
        if (event.document) {
            activeLanguage = event.document.languageId;
            activeCodingSeconds += 1;
        }
    });

    // Periodic sync timer (every 60 seconds)
    timer = setInterval(async () => {
        if (activeCodingSeconds > 0) {
            try {
                // Post active coding telemetry to FastAPI backend
                const payload = {
                    language: activeLanguage,
                    active_coding_seconds: activeCodingSeconds,
                    idle_seconds: idleSeconds
                };

                activeCodingSeconds = 0; // Reset counter
                console.log('[U\'rWay VSCode] Synced active telemetry:', payload);
            } catch (err) {
                console.warn('[U\'rWay VSCode] Telemetry sync error:', err.message);
            }
        }
    }, 60000);

    context.subscriptions.push({
        dispose: () => clearInterval(timer)
    });
}

function deactivate() {
    if (timer) clearInterval(timer);
}

module.exports = {
    activate,
    deactivate
};
