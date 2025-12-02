using Avalonia.Controls;
using Avalonia.LogicalTree;
using Avalonia.Media;
using Avalonia.VisualTree;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager
{
    internal class Translator
    {
        //string? _currentLocale = null;
        //string? CurrentLocale { get { return _currentLocale; } set { if (_currentLocale != value) { _currentLocale = value; /*Translate();*/ } } }
        Dictionary<int, string> originalValues = new Dictionary<int, string>();
        Dictionary<int, string> originalTooltipValues = new Dictionary<int, string>();

        public void Translate(Control root, string localeCode)
        {
            foreach (var control in FindAllTextControls(root))
            {
                if (ToolTip.GetTip(control) is string tooltipText)
                {
                    if (!originalTooltipValues.ContainsKey(control.GetHashCode()))
                        originalTooltipValues.Add(control.GetHashCode(), tooltipText);

                    ToolTip.SetTip(control, "TooltipTest");

                }


                if (control is Label label && label.Content is string text)
                {
                    if (!originalValues.ContainsKey(control.GetHashCode()))
                        originalValues.Add(control.GetHashCode(), text);

                    label.Content = "Test";
                }
                else if (control is TextBlock textBlock && textBlock.Text != null)
                {
                    if (!originalValues.ContainsKey(control.GetHashCode()))
                        originalValues.Add(control.GetHashCode(), textBlock.Text);

                    textBlock.Text = "Test2";
                }
                /*else if (control is TextBox textBox && textBox.Text != null)
                {
                    if (!originalValues.ContainsKey(control.GetHashCode()))
                        originalValues.Add(control.GetHashCode(), textBox.Text);

                    textBox.Text = "Test3";
                }*/
                else if (control is Button button && button.Content is string buttonText)
                {
                    if (!originalValues.ContainsKey(control.GetHashCode()))
                        originalValues.Add(control.GetHashCode(), buttonText);

                    button.Content = "Test4";
                }
                else
                    continue; // Shouldn't happen, IsTextControl() should have rejected it


            }

        }

        static IEnumerable<Control> FindAllTextControls(Control parent)
        {
            if (IsTextControl(parent))
                yield return parent;

            foreach (var descendant in FindAllControls(parent))
            {
                if (IsTextControl(descendant))
                    yield return descendant;
            }
        }

        static IEnumerable<Control> FindAllControls(Control parent)
        {
            foreach (var child in parent.GetLogicalChildren().OfType<Control>())
            {
                yield return child;
                foreach (var subchild in FindAllControls(child))
                {
                    yield return subchild;
                }
            }
        }

        static bool IsTextControl(Control control)
        {
            if (control is Label label && label.Content is string text)
                return true;
            else if (control is TextBlock textBlock && textBlock.Text != null)
                return true;
            /*else if (control is TextBox textBox && textBox.Text != null)
                return true;*/
            else if (control is Button button && button.Content is string buttonText)
                return true;
            else 
                return false;
        }

        
    }
}
