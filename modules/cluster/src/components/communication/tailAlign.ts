const TAIL_ALIGNABLE_OPERATOR_NAMES = ['allreduce', 'alltoall', 'allgather'];

function normalizeOperatorName(operatorName: string): string {
    return operatorName.toLowerCase().replace(/[^a-z]/g, '');
}

export function shouldShowTailAlignTip(operatorName: string): boolean {
    const normalizedName = normalizeOperatorName(operatorName);
    return TAIL_ALIGNABLE_OPERATOR_NAMES.some(name => normalizedName.includes(name));
}
