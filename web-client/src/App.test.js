import { render, screen } from '@testing-library/react';
import App from './App';

test('renders chess login title', () => {
  render(<App />);
  const titleElement = screen.getByText(/CHESS GAME/i);
  expect(titleElement).toBeInTheDocument();
});
