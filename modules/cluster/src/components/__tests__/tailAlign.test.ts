import { shouldShowTailAlignTip } from '../communication/tailAlign';

describe('shouldShowTailAlignTip', () => {
    it('returns true for tail-alignable communication operators', () => {
        expect(shouldShowTailAlignTip('AllReduce')).toBe(true);
        expect(shouldShowTailAlignTip('HcomAllReduce')).toBe(true);
        expect(shouldShowTailAlignTip('all_reduce')).toBe(true);

        expect(shouldShowTailAlignTip('AllGather')).toBe(true);
        expect(shouldShowTailAlignTip('HcomAllGather')).toBe(true);
        expect(shouldShowTailAlignTip('all_gather')).toBe(true);

        expect(shouldShowTailAlignTip('AllToAll')).toBe(true);
        expect(shouldShowTailAlignTip('AlltoAll')).toBe(true);
        expect(shouldShowTailAlignTip('HcomAllToAll')).toBe(true);
        expect(shouldShowTailAlignTip('all_to_all')).toBe(true);
    });

    it('returns false for operators that should not show tail alignment tip', () => {
        expect(shouldShowTailAlignTip('Send')).toBe(false);
        expect(shouldShowTailAlignTip('HcomSend')).toBe(false);
        expect(shouldShowTailAlignTip('Recv')).toBe(false);
        expect(shouldShowTailAlignTip('Receive')).toBe(false);
        expect(shouldShowTailAlignTip('HcomReceive')).toBe(false);
        expect(shouldShowTailAlignTip('Broadcast')).toBe(false);
    });
});
