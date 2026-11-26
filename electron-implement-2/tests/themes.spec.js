const { test, expect } = require('@playwright/test');
const registry = require('../src/themes/registry.json');

test('registry has unique ids, the default theme and valid modes', () => {
  const ids = registry.map((theme) => theme.id);
  expect(new Set(ids).size).toBe(ids.length);
  expect(ids).toContain('paper');
  for (const theme of registry) {
    expect(['light', 'dark']).toContain(theme.mode);
    expect(theme.name.length).toBeGreaterThan(0);
  }
});
