const { defineConfig } = require('@playwright/test');

// Every test run uses an in-memory database and in-memory settings. Nothing is
// read from or written to the real USERPROFILE data folder.
process.env.WORDLOCK_USE_TEST_DATA = '1';

module.exports = defineConfig({
  testDir: './tests',
  timeout: 30_000,
  fullyParallel: false,
  workers: 1,
  reporter: 'list',
  use: {
    // Tests always use the mock; npm start uses the real ntdll implementation.
    launchOptions: {
      env: {
        ...process.env,
        WORDLOCK_CHROME_FREEZE_MOCK: '1',
        WORDLOCK_USE_MOCK_AUTO_START: '1',
        WORDLOCK_USE_TEST_DATA: '1',
      },
    },
  },
});
