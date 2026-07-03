# Contributing to Mindstudio Insight

Thank you for considering contributing to Mindstudio Insight! We welcome contributions in any form-whether it's bug fixes, feature enhancements, documentation improvements, or any feedback suggestions. Whether you're an experienced developer or a first-time open source project, your help is invaluable.

Your support can come in many forms:

 * Report problems or unexpected behavior.
 * Recommend or implement new functions.
 * Improve or expand the document.
 * Review the Pull Request and assist other contributors
 * Share recommendations: Introduce Mind Studio Insight in a blog post, social media, or point to a warehouse project.

We look forward to your participation!

# Find issues that can contribute

Looking for a new issue entry point? You can view the following topics:

 * [good-first-issue](https://gitcode.com/Ascend/msinsight/issues?state=all&scope=all&page=1&categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:22797,%22name%22:%22good-first-issue%22%257D%255D,%22label%22:%22good-first-issue%22%257D%255D)    
 * [help-wanted](https://gitcode.com/Ascend/msinsight/pulls?categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:22796,%22name%22:%22help-wanted%22%257D%255D,%22label%22:%22help-wanted%22%257D%255D&state=opened&scope=all&page=1)    
 * In addition to the two above-mentioned beginner friendly issues, we also offer other[Issue Template](https://gitcode.com/Ascend/.gitcode/tree/master/.gitcode/ISSUE_TEMPLATE)    For reference.
 * In addition, you can also access the[RFC](https://gitcode.com/Ascend/msinsight/issues?state=all&scope=all&page=1&categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:25328,%22name%22:%22rfc%22%257D%255D,%22label%22:%22rfc%22%257D%255D)    And to the[Roadmap](https://gitcode.com/Ascend/msinsight/issues?state=all&scope=all&page=1&categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:22807,%22name%22:%22roadmap%22%257D%255D,%22label%22:%22roadmap%22%257D%255D)    To understand the development plan and planning.

# Pull Requests and Code Reviews

Thank you for submitting the PR! To optimize the review process, follow the guidelines below:

Follow our Pull Request[Templates and Specifications](.gitcode/PULL_REQUEST_TEMPLATE.md)    

Comply with pre-commit[Code specification check](./docs/en/developer_guide/development_guide.md)    Please ensure that all the checks are passed before submitting the PR.

If the client functions are modified, update the corresponding user and developer documents accordingly.

Add or update tests in the CI workflow. If the test is not required, please describe the reason.

After the preceding preparations are complete, submit the code and run the compile command to trigger the robot compilation pipeline.

After the pipeline compilation is successful, contact.[Warehouse management and maintenance members](https://gitcode.com/Ascend/msinsight/member)    Review and Incorporate

# License

Refer to the[LICENSE](./License)    file for full details.

# Build and test

Before submitting a PR, you are advised to set up a local development environment and build`insight`and run the relevant tests.

**Developer test requirements: When the backend code is incorporated, developer test requirements must be met.**

1. The backend DT uses the test framework GoogleTest. The DT code is stored in server/src/test. In Linux, run the following command in the build directory:`bash cpp_coverage.sh`Coverage is generated. The row coverage rate must reach 80% and the branch coverage rate must reach 60%. When the new feature code is incorporated into the backend, DT must be supplemented. For details, see.[Development Guide](./docs/en/developer_guide/development_guide.md)    Section 3.3.3 of.

**Pre-smoke test requirements: Pre-smoke test requirements are required when the front-end or back-end code is incorporated.**

1. Pre-smoke test is an end-to-end test used to verify that the main functions of the software are running properly. It involves the front end and back end. Pre-smoke tests use the test framework Playwright. For details, see.[Development Guide](./docs/en/developer_guide/development_guide.md)Section 3.5 of.

## PR Title and Category

Only specific types of PRs will be approved. Please add the appropriate prefix before the PR title to clarify the PR type. Use one of the following categories:

 * `[Platform]`\: new features, optimizations, or bug fixes related to the base platform.
 * `[Common]`\: new functions, optimizations, or bug fixings related to common modules.
 * `[Timeline]`\: System Tuning - New features, optimizations, or bug fixes related to clustering.
 * `[Memory]`\: System Tuning - New features, optimizations, or bug fixes related to memory.
 * `[Operator]`\: System Tuning-New functions, optimizations, or bug fixings related to operators.
 * `[MemScope]`\: System Tuning - New features, optimizations, or bug fixes related to memory details.
 * `[Cluster]`\: System Tuning - New features, optimizations, or bug fixes related to cluster details.
 * `[RL]`\: System Tuning - New features, optimizations, or bug fixes related to reinforcement learning.
 * `[Advisor]`\: System Tuning - Expert recommendations for relevant new features, optimizations, or bug fixes.
 * `[Source]`\: new functions, optimizations, or bug fixings related to operator optimization.
 * `[Servitization]`\: new functions, optimization, or bug fixing related to service-oriented optimization.

## Commit Requirement

To keep the commit records clear, make sure that each PR contains only one commit. If your PR currently contains multiple commitments, use any of the following methods (including but not limited to) to merge them into a single commit before submitting. (Although GitCode provides the `Squash merge` The option of organizing PRs into a single, concise commit in advance is still considered a best practice and is popular with committers.)

### Method 1: Interactive base change (recommended)

 * View the latest commit files to be merged (for example, the latest three).

```bash
git log --oneline -n 3
```

 * Start interactive rebase (`N`Replace it with the number of commit files to be combined.

```bash
git rebase -i HEAD~N
```

 * In the open editor:
    
     * Retain the first commit`pick`.
     * Change the rest of the`pick`Changed to`squash`(or short for`s`).
 * Save and close. A new window opens for you to write the combined concise, meaningful commit information.
 * Push the updated branches forcibly (only for your own feature branches):

```bash
git push --force-with-lease origin your-branch-name
```

### Method 2: reset + new commit

```bash
#Obtain the latest target branch to be incorporated, for example, main.
git fetch origin main

#Soft-reset to the trunk branch -- This saves all changes and returns to the staging area.
git reset --soft origin/main

#Commit all changes as a new commit
git commit -m "feat: concise description of your change"

#Force Push to Update PR Branch
git push --force-with-lease origin your-branch-name
```

> Hint: If you are unsure which target branch should be based on, check the default branch for the repository or consult the Maintainer.
> Warning: Do not force push on shared or protected branches.

# Thank you for

We appreciate your contributions to MindStudio Insight. Every effort you make makes this project stronger and easier to use. I wish you a happy creation and a happy programming!
