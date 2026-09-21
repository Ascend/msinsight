/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2023 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
module.exports = {
  root: true,
  env: {
    node: true,
  },
  extends: [
    'standard-with-typescript',
    'plugin:import/errors',
    'plugin:react/recommended',
    'plugin:react-hooks/recommended',
  ],
  plugins: [
    '@typescript-eslint',
    'react',
    'import',
  ],
  parser: '@typescript-eslint/parser',
  parserOptions: {
    ecmaVersion: 2021,
    sourceType: 'module',
    ecmaFeatures: {
      jsx: true,
    },
    project: './tsconfig.json',
  },
  settings: {
    'import/resolver': {
      node: {
        extensions: [
          '.js',
          '.jsx',
          '.ts',
          '.tsx',
        ],
      },
    },
    react: {
      version: "detect",
    }
  },
  rules: {
    'no-console': process.env.NODE_ENV === 'production' ? 'warn' : 'off',
    'no-debugger': process.env.NODE_ENV === 'production' ? 'warn' : 'off',
    indent: 'off',
    '@typescript-eslint/indent': [
      'error',
      4,
    ],
    quotes: 'off',
    '@typescript-eslint/quotes': [
      'error',
      'single',
    ],
    semi: 'off',
    '@typescript-eslint/semi': [
      'error',
      'always',
    ],
    'space-before-function-paren': 'off',
    '@typescript-eslint/space-before-function-paren': 'off',
    'no-unused-vars': 'off',
    '@typescript-eslint/no-unused-vars': ['error'],
    'comma-dangle': 'off',
    '@typescript-eslint/comma-dangle': [
      'error',
      'always-multiline',
    ],
    'max-lines': [
      'error',
      2000,
    ],
    'no-void': [
      'error',
      {
        allowAsStatement: true,
      },
    ],
    'no-useless-return': 'error',
    'react/prop-types': 0,
    'jsx-quotes': 1,
    'object-curly-spacing': ['error', 'always'],
    'array-bracket-spacing': ['error', 'never', { 'singleValue': false }],
    'react-hooks/exhaustive-deps': 0,
    '@typescript-eslint/member-delimiter-style': ['error', {
      "multiline": {
        "delimiter": "semi",
        "requireLast": true
      },
      "singleline": {
        "delimiter": "semi",
        "requireLast": false
      },
    }],
    '@typescript-eslint/consistent-type-definitions': ['off'],
    'n/no-callback-literal': ['off'],
    'accessor-pairs': ['off'],
    '@typescript-eslint/triple-slash-reference': ['off'],
    '@typescript-eslint/restrict-plus-operands': ['error', { allowAny: true }],
    '@typescript-eslint/prefer-function-type': ['off'],
    '@typescript-eslint/consistent-type-assertions': ['off'],
    '@typescript-eslint/restrict-template-expressions': ['off'],
    '@typescript-eslint/no-misused-promises': ['off'],
    '@typescript-eslint/promise-function-async': ['off'],
    '@typescript-eslint/no-floating-promises': ['off'],
    '@typescript-eslint/return-await': ['off'],
    '@typescript-eslint/strict-boolean-expressions': ['off', {
      allowNullableObject: true,
      allowNullableBoolean: true,
    }],
    '@typescript-eslint/no-unnecessary-type-assertion': ['off'],
    '@typescript-eslint/no-var-requires': 0,
    '@typescript-eslint/adjacent-overload-signatures':['warn'],
    'import/no-unresolved':['error', { "ignore": ["lib", 'react', 'react-dom', '@/'] }],
    'import/namespace': 'off'
  },
  overrides: [
    {
      files: ['**/*.ts', '**/*.tsx'],
      rules: {
        'no-undef': 'off',
      },
    },
  ],
  'ignorePatterns': ['node_modules/', 'build/'],
}
