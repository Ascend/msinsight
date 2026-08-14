/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    COMMAND_ERROR_CODES,
    CommandError,
    validateCommandDefinition,
    type CommandDefinition,
    type CommandHandler,
} from '@insight/lib/FrontendAgentCommand';

export interface BuiltinCommandEntry {
    source: 'builtin';
    definition: CommandDefinition;
}

export interface GlobalCommandEntry {
    source: 'global';
    definition: CommandDefinition;
    handler: CommandHandler;
}

export interface ModuleCommandEntry {
    source: 'module';
    definition: CommandDefinition;
    moduleId: string;
}

export type VisibleCommandEntry = BuiltinCommandEntry | GlobalCommandEntry | ModuleCommandEntry;

const HELP_DEFINITION: CommandDefinition = {
    name: 'help',
    title: 'Help',
    description: 'List available commands or show the full definition of one command.',
    inputSchema: {
        type: 'object',
        properties: { command: { type: 'string', minLength: 1 } },
        additionalProperties: false,
    },
};

const OBSERVE_DEFINITION: CommandDefinition = {
    name: 'observe',
    title: 'Observe page',
    description: 'Observe the current MindStudio Insight page and active module.',
    inputSchema: { type: 'object', properties: {}, additionalProperties: false },
};

export class CommandCatalog {
    private readonly globals = new Map<string, GlobalCommandEntry>();
    private readonly modules = new Map<string, Map<string, ModuleCommandEntry>>();
    private readonly builtins = new Map<string, BuiltinCommandEntry>([
        ['help', { source: 'builtin', definition: HELP_DEFINITION }],
        ['observe', { source: 'builtin', definition: OBSERVE_DEFINITION }],
    ]);

    registerGlobal(definition: CommandDefinition, handler: CommandHandler): () => void {
        validateCommandDefinition(definition);
        if (!definition.name.startsWith('framework.')) {
            throw invalid(`Framework command '${definition.name}' must use the 'framework.' namespace.`);
        }
        this.ensureNameAvailable(definition.name);
        const entry: GlobalCommandEntry = { source: 'global', definition, handler };
        this.globals.set(definition.name, entry);
        return () => {
            if (this.globals.get(definition.name) === entry) this.globals.delete(definition.name);
        };
    }

    replaceModule(moduleId: string, definitions: CommandDefinition[]): void {
        const commands = new Map<string, ModuleCommandEntry>();
        definitions.forEach((definition) => {
            validateCommandDefinition(definition);
            if (!definition.name.startsWith(`${moduleId}.`)) {
                throw invalid(`Module command '${definition.name}' must use the '${moduleId}.' namespace.`);
            }
            if (commands.has(definition.name) || this.globals.has(definition.name) || this.builtins.has(definition.name)) {
                throw invalid(`Command '${definition.name}' is already registered.`);
            }
            commands.set(definition.name, { source: 'module', moduleId, definition });
        });
        this.modules.set(moduleId, commands);
    }

    removeModule(moduleId: string): void {
        this.modules.delete(moduleId);
    }

    listVisible(activeModule: string): CommandDefinition[] {
        return [
            ...[...this.builtins.values()].map(({ definition }) => definition),
            ...[...this.globals.values()].map(({ definition }) => definition),
            ...[...(this.modules.get(activeModule)?.values() ?? [])].map(({ definition }) => definition),
        ];
    }

    getVisible(name: string, activeModule: string): VisibleCommandEntry | undefined {
        return this.builtins.get(name) ?? this.globals.get(name) ?? this.modules.get(activeModule)?.get(name);
    }

    private ensureNameAvailable(name: string): void {
        if (this.builtins.has(name) || this.globals.has(name) || [...this.modules.values()].some(commands => commands.has(name))) {
            throw invalid(`Command '${name}' is already registered.`);
        }
    }
}

const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID,
    message,
    retryable: false,
});
