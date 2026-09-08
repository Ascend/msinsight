import connector from '../connection';
import { actionRemoveTimeRangeAnalysis, actionTimeRangeAnalysis } from './actionTimeRangeAnalysis';

jest.mock('../connection', () => ({
    __esModule: true,
    default: { send: jest.fn() },
}));

const createSession = (): any => ({
    selectedRange: [100, 200],
    selectedUnits: [{}],
    isTimeAnalysisMode: false,
    mKeyRender: false,
    mMaskRange: [],
    timeAnalysisRange: [100, 200],
});

beforeEach(() => jest.clearAllMocks());

test.each([
    ['selects', actionTimeRangeAnalysis, [100, 200]],
    ['clears', actionRemoveTimeRangeAnalysis, null],
])('%s the Timeline range for other modules', (_name, action, expectedRange) => {
    const session = createSession();
    action.perform(session);

    expect(connector.send).toHaveBeenCalledWith({
        event: 'updateSession',
        body: { timeAnalysisRange: expectedRange },
    });
});
